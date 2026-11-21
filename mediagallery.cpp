#include "mediagallery.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

MediaGallery::MediaGallery(QWidget *parent)
    : QDockWidget(tr("Media Gallery"), parent)
{
    QWidget *dockWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(dockWidget);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    refreshButton = new QPushButton(tr("Scan Media"), this);
    openButton = new QPushButton(tr("Open Selected"), this);
    btnLayout->addWidget(refreshButton);
    btnLayout->addWidget(openButton);
    layout->addLayout(btnLayout);

    mediaList = new QListWidget(this);
    mediaList->setIconSize(QSize(48, 48));
    layout->addWidget(mediaList);

    dockWidget->setLayout(layout);
    setWidget(dockWidget);

    connect(refreshButton, &QPushButton::clicked, this, &MediaGallery::onRefreshClicked);
    connect(openButton, &QPushButton::clicked, this, &MediaGallery::onOpenSelectedClicked);
    connect(mediaList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        emit mediaItemClicked(item);
    });
}

void MediaGallery::refreshGallery()
{
    QWidget *parentWidget = this->parentWidget();
    if (!parentWidget) return;

    QWebEngineView *webView = parentWidget->findChild<QWebEngineView *>();
    if (!webView || !webView->page()) return;

    QString javascript = R"(
        (() => {
            try {
                const mediaElements = [];
                const selectors = [
                    'img[src*="mmg.whatsapp.net"]',
                    'img[src*="pps.whatsapp.net"]',
                    'img[src^="blob:"]',
                    'video[src]',
                    'video source[src]',
                    '[data-testid*="image"] img',
                    '[data-testid*="video"] video'
                ];

                selectors.forEach(selector => {
                    document.querySelectorAll(selector).forEach(el => {
                        const src = el.src || el.getAttribute('src');
                        if (src && !src.includes('emoji') && !src.includes('data:image/svg')) {
                            mediaElements.push(src);
                        }
                    });
                });

                return [...new Set(mediaElements)];
            } catch (e) {
                console.error('MediaGallery Error:', e);
                return [];
            }
        })()
    )";

    webView->page()->runJavaScript(javascript, [this](const QVariant &result) {
        mediaList->clear();

        QStringList urls;
        if (result.canConvert<QStringList>()) {
            urls = result.toStringList();
        } else {
            QJsonArray jsonArray = QJsonDocument::fromJson(result.toString().toUtf8()).array();
            for (const auto &value : jsonArray) {
                urls.append(value.toString());
            }
        }

        qDebug() << "MediaGallery: Found" << urls.size() << "media items";

        for (const QString &urlString : urls) {
            if (!urlString.isEmpty()) {
                QString fileName = QFileInfo(QUrl(urlString).path()).fileName();
                if (fileName.isEmpty() || urlString.startsWith("blob:")) {
                    fileName = tr("Media Item (%1)").arg(mediaList->count() + 1);
                }

                QListWidgetItem *item = new QListWidgetItem(fileName);
                item->setData(Qt::UserRole, urlString);
                item->setToolTip(urlString);
                item->setIcon(QIcon(":/icons/gallery.png"));
                mediaList->addItem(item);
            }
        }

        if (urls.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(tr("No media found in view"));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            mediaList->addItem(item);
        }
    });
}

void MediaGallery::onRefreshClicked()
{
    refreshGallery();
}

void MediaGallery::onOpenSelectedClicked()
{
    QListWidgetItem *item = mediaList->currentItem();
    if (item) {
        emit mediaItemClicked(item);
    }
}
