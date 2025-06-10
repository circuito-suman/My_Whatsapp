#include "mywhatsappwindow.h"
#include <QApplication>  // Added
#include <QWebEngineProfile>
#include <QWebEngineNotification>  // Added
#include <QWebEnginePermission>  //
#include <QWebEngineSettings>  //
#include <QWebEngineDownloadRequest>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <QMenu>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QWebEngineView>
#include <QWebEnginePage>
#include<QWebChannel>
#include <QSystemTrayIcon>

MyWhatsappWindow::MyWhatsappWindow(QWidget *parent)
    : QMainWindow(parent)/*, ui(new Ui::MyWhatsappWindow)  // Initialize here*/
{
    // ui->setupUi(this);  // Now works



    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/whatsapp.png"));  // Must use absolute
    trayMenu = new QMenu(this);
    trayMenu->addAction("Show/Hide", this, &MyWhatsappWindow::toggleVisibility);
    trayMenu->addAction("Quit", qApp, &QApplication::quit);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    //

    connect(trayIcon, &QSystemTrayIcon::messageClicked,
            this, &MyWhatsappWindow::onMessageClicked);
    //

    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // Add at startup
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

    QDir().mkpath(appDataPath);
    qDebug() << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);


    // // Persistent cookies
    // QWebEngineProfile::defaultProfile()->settings()->setAttribute(
    //     QWebEngineSettings::JavascriptEnabled, true
    //     );

    // QWebEngineProfile::defaultProfile()->setNotificationPresenter(
    //     [this](std::unique_ptr<QWebEngineNotification> notification) {
    //         trayIcon->showMessage(notification->title(), notification->message());
    //         notification->show();
    //     });






    // Check if codecs are enabled
    qDebug() << "Proprietary codecs enabled:"
             << QWebEngineProfile::defaultProfile()->settings()->testAttribute(
                    QWebEngineSettings::PlaybackRequiresUserGesture
                    );



    QWebEngineProfile *profile = new QWebEngineProfile("MyWhatsappProfile", this);
    profile->setPersistentStoragePath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/storage"
        );
    profile->setCachePath(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        );
    profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    profile->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);

    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    profile->setHttpCacheMaximumSize(50 * 1024 * 1024); // 50 MB



    profile->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    // profile->settings()->setUnknownUrlSchemePolicy(
    //     QWebEngineSettings::AllowUnknownUrlSchemesFromUserInteraction
    //     );
    // profile->settings()->setAttribute(
    //     QWebEngineSettings::AllowRunningInsecureContent,
    //     false
    //     );



    profile->settings()->setAttribute(
        QWebEngineSettings::PlaybackRequiresUserGesture, false
        );


    profile->setNotificationPresenter(
        [this](std::unique_ptr<QWebEngineNotification> notification) {
            QString cleanTitle = notification->title().replace(QRegularExpression("\\(\\d+\\)\\s*"), "");

            // Only show if not already displayed
            if(!activeNotifications.contains(cleanTitle)) {
                trayIcon->showMessage(
                    cleanTitle,
                    notification->message(),
                    QSystemTrayIcon::MessageIcon::Information, // Uses system-standard icon
                    5000 // 5-second timeout
                    );
                // activeNotifications.insert(cleanTitle, QSystemTrayIcon::Information);
            }
            notification->close();  // Immediately close web notification
        });


    webView = new QWebEngineView(this);
    // webView->setPage(new QWebEnginePage(profile, webView));
    webView->setPage(new LinkHandler(profile, webView));  // Set custom page

    webView->page()->profile()->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/125.0.0.0 Safari/537.36"
        );

    // webView->load(QUrl("https://web.whatsapp.com"));
    QTimer::singleShot(0, this, [this]() {
        webView->load(QUrl("https://web.whatsapp.com"));
    });

    connect(webView, &QWebEngineView::renderProcessTerminated,
            this, &MyWhatsappWindow::onRenderProcessTerminated);

     setCentralWidget(webView);
//

     connect(webView, &QWebEngineView::loadFinished, [this](bool ok) {
         if(ok) {
             // All your JavaScript injections here
             webView->page()->runJavaScript(R"(
            // DOM safety checks
            const videoElem = document.querySelector('video');
            if(videoElem) {
                videoElem.setAttribute('type', 'video/webm');
            }

            // Audio context setup
            document.body.addEventListener('click', () => {
                if(!window.audioContext) {
                    window.audioContext = new (window.AudioContext || window.webkitAudioContext)();
                }
                if(window.audioContext.state === 'suspended') {
                    window.audioContext.resume();
                }
            }, { once: true });

            // Link handling
            document.addEventListener('click', function(e) {
                if (e.target.tagName === 'A') {
                    if (e.ctrlKey) {
                        window.open(e.target.href, '_system');
                        e.preventDefault();
                    } else if (e.shiftKey) {
                        window.open(e.target.href, '_blank');
                        e.preventDefault();
                    }
                }
            });
        )");
         }
     });



    // 2. Create media dock widget (initially hidden)
    mediaDock = new QDockWidget("Media Gallery", this);
    mediaDock->setAllowedAreas(Qt::RightDockWidgetArea);
    mediaDock->setFeatures(QDockWidget::DockWidgetClosable);

    // 3. Create media list widget
    QWidget *mediaWidget = new QWidget(this);
    QVBoxLayout *mediaLayout = new QVBoxLayout(mediaWidget);

    QPushButton *refreshButton = new QPushButton("Refresh Media", this);
    mediaList = new QListWidget(this);

    mediaLayout->addWidget(refreshButton);
    mediaLayout->addWidget(mediaList);
    mediaDock->setWidget(mediaWidget);

    // 4. Add dock to main window (initially hidden)
    addDockWidget(Qt::RightDockWidgetArea, mediaDock);
    mediaDock->hide();

    // 5. Create toggle action with icon
    toggleMediaAction = mediaDock->toggleViewAction();
    toggleMediaAction->setIcon(QIcon(":/icons/gallery.png")); // Add this icon
    toggleMediaAction->setText("Media Gallery");


    // Connect signals
    connect(refreshButton, &QPushButton::clicked,
            this, &MyWhatsappWindow::refreshMediaGallery);


    QToolBar *titleButtons = new QToolBar(this);
    titleButtons->setMovable(false);
    titleButtons->setFloatable(false);
    titleButtons->setIconSize(QSize(16, 16));
    titleButtons->addAction(toggleMediaAction);
    addToolBar(Qt::TopToolBarArea, titleButtons);

    // Style the toolbar to blend with title bar
    titleButtons->setStyleSheet("QToolBar { border: none; background: transparent; }");

    // Connect toggle action to a resize handler
    connect(toggleMediaAction, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            // Sidebar is now visible, adjust main window if needed
            mediaDock->show();
        } else {
            // Sidebar is now hidden
            mediaDock->hide();
        }
        // Allow layout to settle
        QTimer::singleShot(100, this, [this]() {
            adjustSize();
        });
    });


    QToolBar *titleBar = new QToolBar(this);
    titleBar->setMovable(false);
    titleBar->setStyleSheet("QToolBar { background: transparent; border: none; }");

    // Export button
    exportButton = new QToolButton(this);
    exportButton->setIcon(QIcon(":/icons/single-view.png"));
    exportButton->setToolTip("Export Chat to CSV");
    titleBar->addWidget(exportButton);

    addToolBar(Qt::TopToolBarArea, titleBar);

    connect(exportButton, &QToolButton::clicked,
            this, &MyWhatsappWindow::exportChatToCSV);


    /////////////////////////////
    ///




    connect(webView->page(), &QWebEnginePage::permissionRequested,
            [](QWebEnginePermission permission) {
                using PT = QWebEnginePermission::PermissionType;
                switch (permission.permissionType()) {
                case PT::Notifications:
                case PT::MediaAudioCapture:
                case PT::MediaVideoCapture:
                case PT::DesktopVideoCapture:
                    permission.grant();
                    break;
                default:
                    permission.deny();
                    break;
                }
            });

    //


//
    connect(webView->page()->profile(), &QWebEngineProfile::downloadRequested,
            this, [this](QWebEngineDownloadRequest *download) {
                // Ask user where to save the file
                QString path = QFileDialog::getSaveFileName(this, "Save File", download->downloadFileName());
                if (path.isEmpty()) {
                    download->cancel();
                    return;
                }
                download->setDownloadDirectory(QFileInfo(path).absolutePath());
                download->setDownloadFileName(QFileInfo(path).fileName());
                download->accept();

                // Create a progress dialog
                QProgressDialog *progress = new QProgressDialog(
                    QString("Downloading %1").arg(download->downloadFileName()),
                    "Cancel", 0, 100, this);
                progress->setWindowModality(Qt::WindowModal);
                progress->setMinimumDuration(0);

                // Update progress bar
                connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
                        this, [progress, download]() {
                            qint64 received = download->receivedBytes();
                            qint64 total = download->totalBytes();
                            if (total > 0)
                                progress->setValue(static_cast<int>(received * 100 / total));
                        });

                // Handle completion
                connect(download, &QWebEngineDownloadRequest::stateChanged,
                        this, [progress, download, this]() {
                            if (download->state() == QWebEngineDownloadRequest::DownloadCompleted) {
                                progress->close();
                                progress->deleteLater();
                                QMessageBox::information(this, "Download Complete",
                                                         QString("File downloaded to:\n%1/%2")
                                                             .arg(download->downloadDirectory(), download->downloadFileName()));
                            } else if (download->state() == QWebEngineDownloadRequest::DownloadCancelled) {
                                QMessageBox::warning(this, "Download Cancelled", "The download was cancelled.");
                            } else {
                                QMessageBox::critical(this, "Download Failed", "The download failed.");
                            }
                        });

                // Handle cancel from progress dialog
                connect(progress, &QProgressDialog::canceled, download, &QWebEngineDownloadRequest::cancel);
            });
    connect(webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if(ok) {
            // Initial DOM safety checks
            webView->page()->runJavaScript(
                "const videoElem = document.querySelector('video');"
                "if(videoElem) {"
                "    videoElem.setAttribute('type', 'video/webm');"
                "}"
                );

            // Media source validation
            webView->page()->runJavaScript(
                "Array.from(document.querySelectorAll('img[src], video[src]'))"
                "    .map(el => el.src || '')"
                "    .filter(src => src && !src.startsWith('blob:') && !src.includes('emoji'));"
                );
        }
    });


    connect(webView, &QWebEngineView::loadFinished,this, [this](bool ok) {
        if (ok) {
            webView->page()->runJavaScript(
               "const audioContext = new AudioContext();"
               "document.querySelector('#startButton').addEventListener('click', () => {"
               "audioContext.resume().then(() => {"
               "audioContext.audioWorklet.addModule('audio-processor.js');"
                "console.log('AudioContext resumed!');"
                        "});"
                    "});"

                );
        }
    });
    // In your page load handler:
    webView->page()->runJavaScript(
        "var videoElem = document.querySelector('video');"
        "if (videoElem) {"
            "videoElem.setAttribute('type', 'video/webm');"
       "}"        );

    // Create the button
    stayOnTopButton = new QToolButton(this);
    stayOnTopButton->setCheckable(true);
    stayOnTopButton->setIcon(QIcon(":/icons/pin.png")); // Add to your resources
    stayOnTopButton->setToolTip("Always on Top");
    connect(stayOnTopButton, &QToolButton::toggled, this, &MyWhatsappWindow::toggleStayOnTop);

    // Add to your title bar (assuming you have a QToolBar called titleBar)
    titleBar->addWidget(stayOnTopButton);

    // Notification handler
    QWebEngineProfile::defaultProfile()->setNotificationPresenter(
        [this](std::unique_ptr<QWebEngineNotification> notification) {
            QString key = notification->title() + notification->message();

            // Only show if not already read
            if(!activeNotifications.contains(key)) {
                trayIcon->showMessage(notification->title(), notification->message());
                activeNotifications.insert(key, QSystemTrayIcon::Information);
            }
            notification->close(); // Close web notification immediately
        });



    connect(webView->page(), &QWebEnginePage::navigationRequested,
            this,
            [this](QWebEngineNavigationRequest &request) {
                QUrl url = request.url();
                if (url.toString().endsWith(".pdf")) {
                    // Example: show a dialog before blocking
                    QMessageBox::information(this, "Blocked", "PDF navigation blocked!");
                    request.reject();
                } else {
                    request.accept();
                }
            });

    connect(webView, &QWebEngineView::loadFinished, [this](bool ok) {
        if(ok) {
            webView->page()->runJavaScript(R"(
            function initElements() {
                const observer = new MutationObserver(() => {
                    const videoElem = document.querySelector('video');
                    if(videoElem) {
                        videoElem.setAttribute('type', 'video/webm');
                        observer.disconnect();
                    }
                });
                observer.observe(document.body, { childList: true, subtree: true });
            }
            initElements();
        )");

        }
    });






    // Handle message read events
    webView->page()->setWebChannel(new QWebChannel(webView->page()));
    webView->page()->webChannel()->registerObject("external", this);

    // In your main window constructor
    QToolButton *printBtn = new QToolButton(this);
    printBtn->setIcon(QIcon(":/icons/print.png"));
    printBtn->setToolTip("Print PDF");
    connect(printBtn, &QToolButton::clicked, [this]() {
        if(currentUrlIsPdf()) {
            webView->page()->printToPdf("output.pdf");
        }
    });

    QToolButton *previewBtn = new QToolButton(this);
    previewBtn->setIcon(QIcon(":/icons/preview.png"));
    previewBtn->setToolTip("Print Preview");
    connect(previewBtn, &QToolButton::clicked, [this]() {
        if(currentUrlIsPdf()) {
            showPrintPreview();
        }
    });

    // Add to existing title bar
    titleBar->addWidget(previewBtn);
    titleBar->addWidget(printBtn);


    //

    // Set custom page
    // Create LinkHandler with your profile
    // LinkHandler *handler = new LinkHandler(profile, this);
    // webView->setPage(handler);

    // Change connections to use explicit LinkHandler* cast
    LinkHandler *handler = qobject_cast<LinkHandler*>(webView->page());
    if(handler) {
        connect(handler, &LinkHandler::externalLinkRequested, this,
                [](const QUrl &url) {
                    QDesktopServices::openUrl(url);
                });

        connect(handler, &LinkHandler::internalLinkRequested,
                [this](const QUrl &url) {
                    webView->page()->runJavaScript(
                        QString("window.open('%1', '_blank')").arg(url.toString()));
                });
    }



    // In MyWhatsappWindow constructor
    QTimer *syncTimer = new QTimer(this);
    connect(syncTimer, &QTimer::timeout, this, [this]() {
        webView->page()->runJavaScript(
        "if(window.Store && window.Store.Msg) {"
            "window.Store.Msg.syncFetchMessages();"
        "}"
            );
    });
    syncTimer->start(30000); // Sync every 30 seconds

    QTimer *cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, [this](){
        activeNotifications.clear();
        // trayIcon->hideMessage();  // Clear all tray messages
        trayIcon->hide();  // Clear all tray messages
        webView->page()->runJavaScript(
            "document.querySelectorAll('video:not([type])').forEach(v => {"
            "    v.setAttribute('type', 'video/webm');"
            "});"
            );

    });
    cleanupTimer->start(300000);  // 5 minutes


}

// Correct implementation with proper member access
void MyWhatsappWindow::notify(const QString &message) {
    QJsonObject obj = QJsonDocument::fromJson(message.toUtf8()).object();
    if(obj["type"] == "messageRead") {
        QString msgId = obj["id"].toString();
        activeNotifications.remove(msgId);  // Now accesses member variable
        trayIcon->hide();  // Now accesses member variable
    }
}


bool MyWhatsappWindow::shouldShowNotification(const QString &title, const QString &message) {
    // Check against user-defined filters (store in QSettings or a list)
    return !title.contains("MutedContact") && !message.contains("spam");
}

void MyWhatsappWindow::toggleStayOnTop(bool checked)
{
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show(); // Re-show to apply the flag change
}


void MyWhatsappWindow::exportChatToCSV()
{
    webView->page()->runJavaScript(
        "(() => {"
        "  const messages = [];"
        "  document.querySelectorAll('[data-id][data-pre-plain-text]').forEach(msg => {"
        "    const meta = msg.getAttribute('data-pre-plain-text').slice(1, -1).split('] ');"
        "    const time = meta[0];"
        "    const sender = meta[1].replace(':', '');"

        "    let content = '';"
        "    const textElem = msg.querySelector('.selectable-text');"
        "    const callElem = msg.querySelector('[data-icon-name='voice-call-incoming-filled']');"

        "    if(textElem) {"
        "      content = textElem.innerText;"
        "    } else if(callElem) {"
        "      content = 'Missed voice call';"
        "    }"

        "    messages.push({"
        "      time: time,"
        "      sender: sender,"
        "      content: content"
        "    });"
        "  });"
        "  return JSON.stringify(messages);"
        "})()",
        [this](const QVariant &result) {
            if(!result.isValid()) {
                QMessageBox::warning(this, "Export Failed", "Could not retrieve chat messages");
                return;
            }

            QJsonArray messages = QJsonDocument::fromJson(result.toString().toUtf8()).array();
            if(messages.isEmpty()) {
                QMessageBox::information(this, "No Messages", "No messages found in current chat");
                return;
            }

            // Get save path
            QString fileName = QFileDialog::getSaveFileName(
                this, "Save Chat History", "", "CSV Files (*.csv)");
            if(fileName.isEmpty()) return;

            // Write CSV
            QFile file(fileName);
            if(!file.open(QIODevice::WriteOnly)) {
                QMessageBox::critical(this, "Error", "Could not create file");
                return;
            }

            QTextStream stream(&file);
            stream << "Time,Sender,Message\n";

            for(const QJsonValue &msg : messages) {
                QJsonObject obj = msg.toObject();
                stream << "\"" << obj["time"].toString().replace("\"", "\"\"") << "\","
                       << "\"" << obj["sender"].toString().replace("\"", "\"\"") << "\","
                       << "\"" << obj["text"].toString().replace("\"", "\"\"") << "\"\n";
            }

            file.close();
            QMessageBox::information(this, "Export Complete",
                                     QString("Exported %1 messages").arg(messages.size()));
        });
}

void MyWhatsappWindow::refreshMediaGallery()
{
    webView->page()->runJavaScript(
        "Array.from(document.querySelectorAll('img[src], video[src]'))"
        ".map(el => el.src)"
        ".filter(src => !src.startsWith('blob:') && !src.includes('emoji'));",
        [this](const QVariant &result) {
            mediaList->clear();
            QJsonArray urls = QJsonDocument::fromJson(result.toString().toUtf8()).array();
            for (const auto &url : urls) {
                QString urlString = url.toString();
                if(!urlString.isEmpty()) {
                    QListWidgetItem *item = new QListWidgetItem(
                        QFileInfo(urlString).fileName().isEmpty() ?
                            "Media File" : QFileInfo(urlString).fileName()
                        );
                    item->setData(Qt::UserRole, urlString);
                    mediaList->addItem(item);
                }
            }
        });
}

void MyWhatsappWindow::showPdfViewer(const QUrl &pdfUrl) {
    QDialog *pdfDialog = new QDialog(this);
    QWebEngineView *pdfView = new QWebEngineView(pdfDialog);

    // Print buttons
    QToolButton *printBtn = new QToolButton(pdfDialog);
    printBtn->setIcon(QIcon(":/icons/print.png"));
    QToolButton *previewBtn = new QToolButton(pdfDialog);
    previewBtn->setIcon(QIcon(":/icons/preview.png"));

    QHBoxLayout *toolbar = new QHBoxLayout;
    toolbar->addWidget(printBtn);
    toolbar->addWidget(previewBtn);

    QVBoxLayout *layout = new QVBoxLayout(pdfDialog);
    layout->addLayout(toolbar);
    layout->addWidget(pdfView);

    // Load PDF
    pdfView->load(pdfUrl);

    // Connect print functions
    connect(printBtn, &QToolButton::clicked, [pdfView]() {
        QString filePath = QFileDialog::getSaveFileName(nullptr, "Save PDF", "", "PDF Files (*.pdf)");
        if(!filePath.isEmpty()) {
            QPageSize pageSize(QPageSize::A4);
            QPageLayout pageLayout(pageSize, QPageLayout::Portrait, QMarginsF());
            pdfView->page()->printToPdf(filePath, pageLayout);
        }
    });

    connect(previewBtn, &QToolButton::clicked, [pdfView]() {
        QPrintPreviewDialog dialog;
        connect(&dialog, &QPrintPreviewDialog::paintRequested,
                [pdfView](QPrinter *printer) {
                    pdfView->print(printer); // Call print on QWebEngineView
                });
        dialog.exec();
    });

    pdfDialog->exec();
}
bool MyWhatsappWindow::currentUrlIsPdf() {
    return webView->url().toString().endsWith(".pdf");
}

void MyWhatsappWindow::showPrintPreview() {
    QPrintPreviewDialog dialog;
    connect(&dialog, &QPrintPreviewDialog::paintRequested,
            [this](QPrinter *printer) {
                webView->print(printer);
            });
    dialog.exec();
}


void MyWhatsappWindow::onMediaItemClicked(QListWidgetItem *item)
{
    QString url = item->data(Qt::UserRole).toString();
    if(!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}



void MyWhatsappWindow::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}

void MyWhatsappWindow::toggleVisibility() {
    setVisible(!isVisible());
    if(isVisible()) activateWindow();
}

void MyWhatsappWindow::updateUnreadCount(const QString &title) {
    // Extract unread count from page title
    if(title.contains('('))
        trayIcon->setToolTip(title.split('(')[1].split(')')[0] + " unread messages");
}


void MyWhatsappWindow::clearStaleNotifications() {
    webView->page()->runJavaScript(
        "(function() {"
        "const unread = window.WAPI.getUnreadMessages();"
        "return unread.map(msg => msg.id.id);"
    "})()",
        [this](const QVariant &result) {
            QJsonArray unreadIds = result.toJsonArray();
            QHash<QString, QSystemTrayIcon::MessageIcon> updated;
            for(auto it = activeNotifications.begin(); it != activeNotifications.end(); ++it) {
                if(unreadIds.contains(it.key())) {
                    updated.insert(it.key(), it.value());
                }
            }
            activeNotifications = updated;
        });
}


void MyWhatsappWindow::onMessageClicked()
{
    // Handle tray notification click logic here
    activateWindow();
    raise();
    setFocus();
}
void MyWhatsappWindow::onRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int code) {
    qWarning() << "Renderer terminated:" << status << "Exit code:" << code;
    // Add recovery logic here
}
