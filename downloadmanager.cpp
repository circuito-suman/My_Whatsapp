#include "downloadmanager.h"
#include "systemintegration.h"
#include <algorithm>
#include <ranges>
#include <format>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QProcess>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>

DownloadManager::DownloadManager(SystemIntegration *systemIntegration, QObject *parent)
    : QObject(parent)
    , m_autoDownload(true)
    , m_systemIntegration(systemIntegration)
{
    // Default to ~/Downloads/WhatsApp
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsPath.isEmpty()) {
        downloadsPath = QDir::homePath() + "/Downloads";
    }
    m_downloadDirectory = downloadsPath + "/WhatsApp";

    // Ensure download directory exists
    QDir().mkpath(m_downloadDirectory);
}

DownloadManager::~DownloadManager() = default;

QString DownloadManager::downloadDirectory() const
{
    return m_downloadDirectory;
}

void DownloadManager::setDownloadDirectory(const QString &dir)
{
    if (!dir.isEmpty()) {
        m_downloadDirectory = dir;
        QDir().mkpath(m_downloadDirectory);
    }
}

bool DownloadManager::autoDownloadEnabled() const
{
    return m_autoDownload;
}

void DownloadManager::setAutoDownloadEnabled(bool enabled)
{
    m_autoDownload = enabled;
}

void DownloadManager::sortHistoryByProgress()
{
    // C++20 std::ranges::sort using C++20 three-way spaceship operator (<=>)
    std::ranges::sort(m_history, [](const DownloadItem &a, const DownloadItem &b) {
        return (a <=> b) < 0;
    });
}

QString DownloadManager::sanitizeFileName(const QString &fileName, const QString &mimeType)
{
    QString cleanName = fileName;

    // Remove path traversal and dangerous characters
    cleanName.remove("..");
    cleanName.remove(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1F]"));
    cleanName = cleanName.trimmed();

    if (cleanName.isEmpty()) {
        QString timeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        cleanName = QString("WhatsApp_Media_%1").arg(timeStr);
    }

    // Ensure valid extension if missing
    if (!cleanName.contains('.')) {
        if (mimeType.contains("jpeg") || mimeType.contains("jpg")) cleanName += ".jpg";
        else if (mimeType.contains("png")) cleanName += ".png";
        else if (mimeType.contains("webp")) cleanName += ".webp";
        else if (mimeType.contains("mp4")) cleanName += ".mp4";
        else if (mimeType.contains("ogg") || mimeType.contains("opus")) cleanName += ".ogg";
        else if (mimeType.contains("mp3")) cleanName += ".mp3";
        else if (mimeType.contains("pdf")) cleanName += ".pdf";
        else cleanName += ".bin";
    }

    return cleanName;
}

QString DownloadManager::getUniqueFilePath(const QString &dir, const QString &baseName)
{
    QFileInfo fileInfo(QDir(dir), baseName);
    QString name = fileInfo.completeBaseName();
    QString ext = fileInfo.suffix();
    if (!ext.isEmpty()) ext = "." + ext;

    QString candidatePath = fileInfo.absoluteFilePath();
    int counter = 1;

    while (QFile::exists(candidatePath)) {
        candidatePath = QString("%1/%2 (%3)%4").arg(dir, name, QString::number(counter++), ext);
    }

    return candidatePath;
}

void DownloadManager::applySecureFilePermissions(const QString &filePath)
{
    // Restrict permissions to 0644 (Read/write by owner, read by group/other, NEVER execute)
    QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ReadGroup | QFileDevice::ReadOther);
}

void DownloadManager::handleDownload(QWebEngineDownloadRequest *download, QWidget *parentWidget)
{
    if (!download) return;

    QString suggestedName = download->suggestedFileName();
    QString mimeType = download->mimeType();
    QString sanitizedName = sanitizeFileName(suggestedName, mimeType);
    bool isMediaPreview = suggestedName.startsWith("__wa_media_play_");

    QString targetDir = isMediaPreview ?
        (QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/temp_media") :
        m_downloadDirectory;
    QDir().mkpath(targetDir);

    QString fullDestinationPath;

    if (isMediaPreview) {
        fullDestinationPath = targetDir + "/" + sanitizedName;
    } else if (m_autoDownload) {
        fullDestinationPath = getUniqueFilePath(targetDir, sanitizedName);
    } else {
        fullDestinationPath = QFileDialog::getSaveFileName(
            parentWidget,
            tr("Save WhatsApp Attachment"),
            targetDir + "/" + sanitizedName
        );
        if (fullDestinationPath.isEmpty()) {
            download->cancel();
            return;
        }
    }

    QFileInfo destInfo(fullDestinationPath);
    download->setDownloadDirectory(destInfo.absolutePath());
    download->setDownloadFileName(destInfo.fileName());
    download->accept();

    if (isMediaPreview) {
        connect(download, &QWebEngineDownloadRequest::stateChanged, this, [this, download, fullDestinationPath]() {
            if (download->state() == QWebEngineDownloadRequest::DownloadCompleted) {
                applySecureFilePermissions(fullDestinationPath);
                qDebug() << "Media preview download completed, offloading to native player:" << fullDestinationPath;
                QString mpv = QStandardPaths::findExecutable("mpv");
                if (!mpv.isEmpty()) {
                    QProcess::startDetached(mpv, {"--title=WhatsApp Media", "--geometry=65%x65%", "--autofit=75%", "--force-window=immediate", fullDestinationPath});
                } else {
                    QString vlc = QStandardPaths::findExecutable("vlc");
                    if (!vlc.isEmpty()) {
                        QProcess::startDetached(vlc, {fullDestinationPath});
                    } else {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(fullDestinationPath));
                    }
                }
            }
        });
        return;
    }

    DownloadItem item;
    item.id = QString::number(download->id());
    item.fileName = destInfo.fileName();
    item.fullPath = fullDestinationPath;
    item.totalBytes = download->totalBytes();
    item.receivedBytes = 0;
    item.progress = 0;
    item.timestamp = QDateTime::currentDateTime();
    item.isCompleted = false;
    item.isFailed = false;
    item.mimeType = mimeType;

    m_history.prepend(item);
    emit downloadStarted(item.fileName);

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, download, fullDestinationPath]() {
        qint64 total = download->totalBytes();
        qint64 received = download->receivedBytes();
        int percent = total > 0 ? static_cast<int>(received * 100 / total) : 0;

        auto it = std::find_if(m_history.begin(), m_history.end(), [&](const DownloadItem &h) {
            return h.fullPath == fullDestinationPath;
        });
        if (it != m_history.end()) {
            it->receivedBytes = received;
            it->totalBytes = total;
            it->progress = percent;
        }
        emit downloadProgress(QFileInfo(fullDestinationPath).fileName(), percent);
    });

    connect(download, &QWebEngineDownloadRequest::stateChanged, this, [this, download, fullDestinationPath]() {
        QString fName = QFileInfo(fullDestinationPath).fileName();

        if (download->state() == QWebEngineDownloadRequest::DownloadCompleted) {
            applySecureFilePermissions(fullDestinationPath);

            auto it = std::find_if(m_history.begin(), m_history.end(), [&](const DownloadItem &h) {
                return h.fullPath == fullDestinationPath;
            });
            if (it != m_history.end()) {
                it->isCompleted = true;
                it->progress = 100;
            }

            qDebug() << "Download completed successfully:" << fullDestinationPath;
            emit downloadCompleted(fName, fullDestinationPath);

            if (m_systemIntegration) {
                m_systemIntegration->showNativeNotification(
                    tr("Download Complete"),
                    tr("Saved: %1").arg(fName),
                    "document-save"
                );
            }
        } else if (download->state() == QWebEngineDownloadRequest::DownloadCancelled) {
            auto it = std::find_if(m_history.begin(), m_history.end(), [&](const DownloadItem &h) {
                return h.fullPath == fullDestinationPath;
            });
            if (it != m_history.end()) {
                it->isFailed = true;
            }
            emit downloadFailed(fName, tr("Cancelled by user"));
        } else if (download->state() == QWebEngineDownloadRequest::DownloadInterrupted) {
            auto it = std::find_if(m_history.begin(), m_history.end(), [&](const DownloadItem &h) {
                return h.fullPath == fullDestinationPath;
            });
            if (it != m_history.end()) {
                it->isFailed = true;
            }
            emit downloadFailed(fName, tr("Download interrupted: %1").arg(download->interruptReasonString()));
        }
    });
}

void DownloadManager::showDownloadsDialog(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("WhatsApp Downloads"));
    dialog.setMinimumSize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *dirLabel = new QLabel(tr("Saving to: %1").arg(m_downloadDirectory), &dialog);
    dirLabel->setWordWrap(true);
    QPushButton *openFolderBtn = new QPushButton(tr("Open Folder"), &dialog);
    connect(openFolderBtn, &QPushButton::clicked, [&]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadDirectory));
    });
    topBar->addWidget(dirLabel, 1);
    topBar->addWidget(openFolderBtn);
    layout->addLayout(topBar);

    QTableWidget *table = new QTableWidget(&dialog);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({tr("File Name"), tr("Size"), tr("Date"), tr("Actions")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->setRowCount(m_history.size());

    for (int i = 0; i < m_history.size(); ++i) {
        const auto &item = m_history.at(i);
        table->setItem(i, 0, new QTableWidgetItem(item.fileName));

        QString sizeStr = item.totalBytes > 0
            ? QString("%1 MB").arg(QString::number(item.totalBytes / (1024.0 * 1024.0), 'f', 2))
            : tr("Unknown");
        table->setItem(i, 1, new QTableWidgetItem(sizeStr));
        table->setItem(i, 2, new QTableWidgetItem(item.timestamp.toString("MMM d, hh:mm")));

        QWidget *actionWidget = new QWidget(table);
        QHBoxLayout *actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(2, 2, 2, 2);

        QPushButton *openFileBtn = new QPushButton(tr("Open"), actionWidget);
        QString itemPath = item.fullPath;
        connect(openFileBtn, &QPushButton::clicked, [itemPath]() {
            if (QFile::exists(itemPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath));
            } else {
                QMessageBox::warning(nullptr, tr("File Not Found"), tr("The file has been moved or deleted."));
            }
        });
        actionLayout->addWidget(openFileBtn);
        table->setCellWidget(i, 3, actionWidget);
    }

    layout->addWidget(table);

    QHBoxLayout *bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"), &dialog);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    bottomBar->addWidget(closeBtn);
    layout->addLayout(bottomBar);

    dialog.exec();
}
