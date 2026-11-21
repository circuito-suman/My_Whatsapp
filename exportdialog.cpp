#include "exportdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <string_view>
#include <span>

ExportDialog::ExportDialog(QWebEngineView *webView, QObject *parent)
    : QObject(parent), webView(webView) {}

void ExportDialog::exportChatToCSV() {
    if (!webView || !webView->page()) return;

    QString script = R"(
        (() => {
            const messages = [];
            const msgNodes = document.querySelectorAll('[data-id]');

            msgNodes.forEach(msg => {
                let sender = 'Unknown';
                let time = '';
                let content = '';

                // Try data-pre-plain-text attribute
                const meta = msg.getAttribute('data-pre-plain-text');
                if (meta) {
                    const cleanMeta = meta.replace(/^\[/, '').replace(/\]\s*$/, '');
                    const parts = cleanMeta.split('] ');
                    if (parts.length >= 2) {
                        time = parts[0];
                        sender = parts[1].replace(/:$/, '').trim();
                    } else {
                        time = cleanMeta;
                    }
                }

                // Check outgoing vs incoming message classes
                if (sender === 'Unknown') {
                    if (msg.classList.contains('message-out') || msg.closest('.message-out')) {
                        sender = 'Me';
                    } else if (msg.classList.contains('message-in') || msg.closest('.message-in')) {
                        sender = 'Contact';
                    }
                }

                // Extract message content
                const textElem = msg.querySelector('.selectable-text, [data-testid="conversation-text"], span.selectable-text');
                if (textElem) {
                    content = textElem.innerText;
                } else if (msg.querySelector('[data-icon*="call"]')) {
                    content = '[Call Event]';
                } else if (msg.querySelector('[data-testid*="image"]')) {
                    content = '[Image Attachment]';
                } else if (msg.querySelector('[data-testid*="video"]')) {
                    content = '[Video Attachment]';
                } else if (msg.querySelector('[data-testid*="audio"]')) {
                    content = '[Voice Message]';
                } else if (msg.querySelector('[data-testid*="document"]')) {
                    content = '[Document File]';
                }

                if (content.length > 0 || time.length > 0) {
                    messages.push({
                        time: time,
                        sender: sender,
                        content: content
                    });
                }
            });

            return JSON.stringify(messages);
        })()
    )";

    webView->page()->runJavaScript(script, [this](const QVariant &result) {
        if (!result.isValid()) {
            QMessageBox::warning(nullptr, tr("Export Failed"), tr("Could not retrieve messages from the active chat."));
            return;
        }

        QJsonArray messages = QJsonDocument::fromJson(result.toString().toUtf8()).array();
        if (messages.isEmpty()) {
            QMessageBox::information(nullptr, tr("No Messages Found"), tr("No messages could be extracted. Please ensure a chat is open and loaded."));
            return;
        }

        QString defaultName = QString("WhatsApp_Chat_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save Chat History"), defaultName, tr("CSV Files (*.csv)"));
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(nullptr, tr("Error"), tr("Could not open file for writing: %1").arg(file.errorString()));
            return;
        }

        // Clamp permissions to owner read/write only (0600) for privacy
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        QTextStream stream(&file);
        stream << "Timestamp,Sender,Message\n";

        // CWE-1236 Defense: Prevent CSV formula injection using C++20 string_view::starts_with
        auto sanitizeCSVCell = [](QString text) {
            if (!text.isEmpty()) {
                QByteArray utf8 = text.toUtf8();
                std::string_view sv(utf8.constData(), static_cast<std::size_t>(utf8.size()));
                if (sv.starts_with('=') || sv.starts_with('+') || sv.starts_with('-') ||
                    sv.starts_with('@') || sv.starts_with('\t') || sv.starts_with('\r')) {
                    text.prepend('\''); // Prefix with apostrophe to force spreadsheet to treat as raw text
                }
            }
            return text;
        };

        for (const auto &msgVal : messages) {
            QJsonObject obj = msgVal.toObject();
            QString time = sanitizeCSVCell(obj["time"].toString().replace("\"", "\"\""));
            QString sender = sanitizeCSVCell(obj["sender"].toString().replace("\"", "\"\""));
            QString content = sanitizeCSVCell(obj["content"].toString().replace("\"", "\"\""));

            stream << "\"" << time << "\",\"" << sender << "\",\"" << content << "\"\n";
        }

        file.close();
        QMessageBox::information(nullptr, tr("Export Complete"), tr("Successfully exported %1 messages to:\n%2").arg(messages.size()).arg(fileName));
    });
}
