#include "mainwindow.h"
#include "webpage.h"
#include "traymanager.h"
#include "mediagallery.h"
#include "exportdialog.h"
#include "systemintegration.h"
#include "downloadmanager.h"
#include "settingsdialog.h"
#include "mprisplayer.h"
#include "mediaschemehandler.h"

#include <QDesktopServices>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineNotification>
#include <QToolBar>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QScreen>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QUrl>
#include <QCloseEvent>
#include <QApplication>
#include <QMessageBox>
#include <QKeySequence>
#include <QSettings>
#include <QDebug>
#include <unistd.h>
#include <malloc.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_profile(nullptr)
    , m_webView(nullptr)
    , m_webPage(nullptr)
    , m_trayManager(nullptr)
    , m_mediaGallery(nullptr)
    , m_exportDialog(nullptr)
    , m_systemIntegration(nullptr)
    , m_downloadManager(nullptr)
    , m_settingsDialog(nullptr)
    , m_mprisPlayer(nullptr)
    , m_mediaSchemeHandler(nullptr)
    , m_stayOnTopButton(nullptr)
    , m_themeButton(nullptr)
    , m_downloadsButton(nullptr)
    , m_settingsButton(nullptr)
    , m_zoomLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_memoryTimer(nullptr)
    , m_forceClose(false)
    , m_isDarkTheme(false)
    , m_currentZoom(1.0)
{
    // Step 1: Immediate geometry setup / restore BEFORE any WebEngine widget is created.
    // This completely eliminates initial layout viewport resizing and responsive jumping.
    QSettings settings;
    QByteArray savedGeom = settings.value("Geometry/MainWindow").toByteArray();
    if (!savedGeom.isEmpty()) {
        restoreGeometry(savedGeom);
    } else {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeom = screen->availableGeometry();
            int w = qMin(1366, static_cast<int>(screenGeom.width() * 0.85));
            int h = qMin(850, static_cast<int>(screenGeom.height() * 0.85));
            resize(w, h);
            move(screenGeom.center() - rect().center());
        } else {
            resize(1280, 800);
        }
    }

    // Step 2: Initialize system integration
    m_systemIntegration = new SystemIntegration(this);

    // Step 3: Initialize download manager
    m_downloadManager = new DownloadManager(m_systemIntegration, this);
    m_downloadManager->setDownloadDirectory(SettingsDialog::getDownloadDirectory());
    m_downloadManager->setAutoDownloadEnabled(SettingsDialog::getAutoDownload());

    // Step 4: Initialize MPRIS2 media controller for Linux desktop
    m_mprisPlayer = new MprisPlayer(this);
    connect(m_mprisPlayer, &MprisPlayer::playPauseRequested, this, [this]() {
        if (m_webPage) m_webPage->triggerToggleMediaPlayback();
    });
    connect(m_mprisPlayer, &MprisPlayer::raiseRequested, this, &MainWindow::toggleVisibility);
    connect(m_mprisPlayer, &MprisPlayer::quitRequested, this, &MainWindow::exitApplication);

    // Step 5: Initialize System Tray
    m_trayManager = new TrayManager(m_systemIntegration, this);
    connect(m_trayManager, &TrayManager::toggleVisibilityRequested, this, &MainWindow::toggleVisibility);
    connect(m_trayManager, &TrayManager::exitRequested, this, &MainWindow::exitApplication);
    connect(m_trayManager, &TrayManager::newChatRequested, this, [this]() {
        if (m_webPage) m_webPage->triggerNewChat();
    });
    connect(m_trayManager, &TrayManager::toggleThemeRequested, this, &MainWindow::toggleTheme);
    connect(m_trayManager, &TrayManager::openDownloadsRequested, this, &MainWindow::openDownloads);
    connect(m_trayManager, &TrayManager::settingsRequested, this, &MainWindow::openSettings);

    // Step 6: Setup UI structure (toolbars, docks) FIRST so layout frame is finalized
    setupUI();

    // Step 7: Setup Profile and WebEngine with pre-allocated layout dimensions
    setupProfile();

    // Step 8: Shortcuts and memory optimization timers
    setupShortcuts();
    setupMemoryOptimization();

    // Environment-specific window optimizations
    m_systemIntegration->optimizeForEnvironment(this);

    // Theme initialization
    int themeMode = SettingsDialog::getThemeMode();
    if (themeMode == 1) {
        applyTheme(true);
    } else if (themeMode == 2) {
        applyTheme(false);
    } else {
        applyTheme(m_systemIntegration->isSystemDarkTheme());
    }

    connect(m_systemIntegration, &SystemIntegration::systemThemeChanged, this, [this](bool isDark) {
        if (SettingsDialog::getThemeMode() == 0) {
            applyTheme(isDark);
        }
    });

    connect(m_systemIntegration, &SystemIntegration::notificationActionInvoked, this, [this](const QString &action) {
        show();
        if (isMinimized()) {
            setWindowState(windowState() & ~Qt::WindowMinimized);
        }
        showNormal();
        raise();
        activateWindow();
        m_systemIntegration->requestAttention(this);

        if (action == "open_downloads") {
            openDownloads();
        }
    });

    // Initial zoom setting
    m_currentZoom = SettingsDialog::getZoomFactor();
    m_webView->setZoomFactor(m_currentZoom);
    updateZoomLabel();
}

MainWindow::~MainWindow()
{
    if (m_memoryTimer) {
        m_memoryTimer->stop();
    }

    if (m_trayManager && m_trayManager->icon()) {
        m_trayManager->icon()->hide();
    }

    // Explicitly destroy WebView and WebPage before profile destruction
    if (m_webView) {
        delete m_webView;
        m_webView = nullptr;
        m_webPage = nullptr;
    }

    qDebug() << "MainWindow destroyed gracefully";
}

void MainWindow::setupProfile()
{
    // Secure persistent storage directory
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    QFile::setPermissions(appDataPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QString profilePath = appDataPath + "/WebEngineProfile";
    QString cachePath = appDataPath + "/WebEngineCache";
    QDir().mkpath(profilePath);
    QDir().mkpath(cachePath);
    QFile::setPermissions(profilePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QFile::setPermissions(cachePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    m_profile = new QWebEngineProfile("WhatsAppDesktopProfile", this);
    m_profile->setPersistentStoragePath(profilePath);
    m_profile->setCachePath(cachePath);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_profile->setHttpCacheMaximumSize(20 * 1024 * 1024); // 20MB disk cache

    // Install wa-media URL Scheme Handler for high-speed binary video offloading, transcoding, and caller avatars
    m_mediaSchemeHandler = new MediaSchemeHandler(m_profile);
    m_profile->installUrlSchemeHandler("wa-media", m_mediaSchemeHandler);
    connect(m_mediaSchemeHandler, &MediaSchemeHandler::incomingCallDetected, this, [this](const QString &callerName, bool isVideo, const QString &avatarPath) {
        m_lastDetectedCallerName = callerName;
        handleIncomingCall(callerName, isVideo, avatarPath);
    });

    // User-Agent: Chrome on Linux for full WebRTC and Calling capabilities
    QString userAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
    m_profile->setHttpUserAgent(userAgent);

    // Disable remote spellcheck dictionary network probing to avoid leaks
    m_profile->setSpellCheckEnabled(false);

    // WebEngine Settings for Performance, Privacy, and WhatsApp WebRTC Calling
    auto *settings = m_profile->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false); // Required for call ringtones & voice notes
    settings->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);

    // Screen sharing in WebRTC calls
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    QWebEngineProfile::defaultProfile()->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);

    // NETWORK LEAK PREVENTION: Limit WebRTC to public interfaces only.
    // Setting this to TRUE stops WebRTC from leaking local LAN IP addresses (192.168.x.x, 10.x.x.x, VPN IPs).
    settings->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true);

    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    settings->setAttribute(QWebEngineSettings::ShowScrollBars, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);

    // WebEngine Notification Presenter
    m_profile->setNotificationPresenter([this](std::unique_ptr<QWebEngineNotification> notification) {
        if (!notification) return;

        if (m_trayManager && m_trayManager->isMuted()) {
            return;
        }

        if (!SettingsDialog::getNotificationsEnabled()) {
            return;
        }

        QUrl origin = notification->origin();
        if (!WebPage::isTrustedHost(origin.host())) {
            qWarning() << "Blocked notification from untrusted origin (leak prevention):" << origin.toString();
            return;
        }

        QString title = notification->title().left(200);
        QString message = notification->message().left(500);

        // Sanitize dangerous HTML entities
        title.remove(QRegularExpression("[<>&\"']"));
        message.remove(QRegularExpression("[<>&\"']"));

        // Extract contact profile picture if available from HTML5 Web Notification
        QString avatarPath;
        QImage iconImage = notification->icon();
        if (!iconImage.isNull()) {
            QString avatarDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/avatars";
            QDir().mkpath(avatarDir);
            avatarPath = avatarDir + QString("/avatar_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
            if (!iconImage.save(avatarPath, "PNG")) {
                avatarPath.clear();
            }
        }

        // Strict incoming call classification
        // Real WhatsApp call notification formats:
        // Case 1: Title is "Incoming voice call" or "Incoming video call" (or localized)
        // Case 2: Message matches regex "John Doe is calling..."
        // Case 3: Message is "Incoming voice call" or "Incoming video call"
        bool isVoiceCall = (title.compare("Incoming voice call", Qt::CaseInsensitive) == 0) ||
                           (message.compare("Incoming voice call", Qt::CaseInsensitive) == 0);
        bool isVideoCall = (title.compare("Incoming video call", Qt::CaseInsensitive) == 0) ||
                           (message.compare("Incoming video call", Qt::CaseInsensitive) == 0);
        static const QRegularExpression callRegex(
            R"(^(.*?)\s+is calling(?:\s+you)?(?:\s+on WhatsApp)?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch callMatch = callRegex.match(message);
        bool isCallingMsg = callMatch.hasMatch();

        bool isCall = isVoiceCall || isVideoCall || isCallingMsg;

        if (isCall) {
            bool isVideo = isVideoCall ||
                           title.contains("video", Qt::CaseInsensitive) ||
                           message.contains("video", Qt::CaseInsensitive);

            QString callerName;
            if (isCallingMsg) {
                callerName = callMatch.captured(1).trimmed();
            } else if (isVoiceCall || isVideoCall) {
                if (title.contains("call", Qt::CaseInsensitive)) {
                    callerName = message.trimmed();
                } else {
                    callerName = title.trimmed();
                }
            }

            if (callerName.isEmpty() || callerName.compare("WhatsApp", Qt::CaseInsensitive) == 0) {
                callerName = tr("WhatsApp Contact");
            }

            handleIncomingCall(callerName, isVideo, avatarPath);
            return;
        }

        if (!SettingsDialog::getShowNotificationPreview()) {
            message = tr("New WhatsApp message received");
        }

        QString iconToUse = (!avatarPath.isEmpty() && QFile::exists(avatarPath)) ? avatarPath : "whatsapp-desktop";
        m_systemIntegration->showNativeNotification(title, message, iconToUse, false);
    });

    // Downloads pipeline
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest *download) {
        m_downloadManager->handleDownload(download, this);
    });

    // Create View and Custom WebPage
    m_webView = new QWebEngineView(this);
    m_webPage = new WebPage(m_profile, m_webView);
    m_webView->setPage(m_webPage);

    connect(m_webView, &QWebEngineView::titleChanged, this, &MainWindow::handleTitleChanged);

    connect(m_webView, &QWebEngineView::loadStarted, this, [this]() {
        if (m_statusLabel) m_statusLabel->setText(tr("Connecting..."));
    });

    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool success) {
        if (success) {
            if (m_statusLabel) m_statusLabel->setText(tr("Connected"));
            applyTheme(m_isDarkTheme);
        } else {
            if (m_statusLabel) m_statusLabel->setText(tr("Connection Failed"));
        }
    });

    // Load WhatsApp now that the container geometry is already settled
    m_webView->load(QUrl("https://web.whatsapp.com"));
    setCentralWidget(m_webView);
}

void MainWindow::setupUI()
{
    setWindowTitle("WhatsApp Desktop");
    setWindowIcon(QIcon(":/icons/whatsapp.png"));

    // Media Gallery Dock Widget
    m_mediaGallery = new MediaGallery(this);
    addDockWidget(Qt::RightDockWidgetArea, m_mediaGallery);
    m_mediaGallery->hide();

    // Export Dialog
    m_exportDialog = new ExportDialog(m_webView, this);

    // Sleek Modern Toolbar
    QToolBar *toolbar = new QToolBar(tr("Main Toolbar"), this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setStyleSheet("QToolBar { background-color: palette(window); border-bottom: 1px solid palette(mid); spacing: 4px; padding: 2px 8px; }"
                           "QToolButton { border: none; border-radius: 4px; padding: 4px; }"
                           "QToolButton:hover { background-color: palette(highlight); color: palette(highlightedText); }"
                           "QToolButton:checked { background-color: palette(midlight); }");

    // New Chat Action
    QToolButton *newChatBtn = new QToolButton(this);
    newChatBtn->setIcon(QIcon(":/icons/single-view.png"));
    newChatBtn->setToolTip(tr("Start New Chat (Ctrl+N)"));
    connect(newChatBtn, &QToolButton::clicked, this, [this]() {
        if (m_webPage) m_webPage->triggerNewChat();
    });
    toolbar->addWidget(newChatBtn);

    // Media Gallery Action
    QAction *galleryAction = m_mediaGallery->toggleViewAction();
    galleryAction->setIcon(QIcon(":/icons/gallery.png"));
    galleryAction->setText(tr("Media Gallery"));
    galleryAction->setToolTip(tr("Toggle Media Gallery"));
    toolbar->addAction(galleryAction);

    // Media Gallery Click Handler
    connect(m_mediaGallery, &MediaGallery::mediaItemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        QString urlStr = item->data(Qt::UserRole).toString();
        if (!urlStr.isEmpty()) {
            QUrl url(urlStr);
            if (url.isValid() && (url.scheme() == "blob" || url.scheme() == "https" || url.scheme() == "http")) {
                QDesktopServices::openUrl(url);
            }
        }
    });

    // Export Chat Button
    QToolButton *exportBtn = new QToolButton(this);
    exportBtn->setIcon(QIcon(":/icons/preview.png"));
    exportBtn->setToolTip(tr("Export Current Chat to CSV"));
    connect(exportBtn, &QToolButton::clicked, this, &MainWindow::onExportTriggered);
    toolbar->addWidget(exportBtn);

    // Downloads Button
    m_downloadsButton = new QToolButton(this);
    m_downloadsButton->setIcon(QIcon(":/icons/print.png"));
    m_downloadsButton->setToolTip(tr("Downloads Manager (Ctrl+J)"));
    connect(m_downloadsButton, &QToolButton::clicked, this, &MainWindow::openDownloads);
    toolbar->addWidget(m_downloadsButton);

    toolbar->addSeparator();

    // Zoom Controls
    QToolButton *zoomOutBtn = new QToolButton(this);
    zoomOutBtn->setText("−");
    zoomOutBtn->setToolTip(tr("Zoom Out (Ctrl+-)"));
    connect(zoomOutBtn, &QToolButton::clicked, this, &MainWindow::zoomOut);
    toolbar->addWidget(zoomOutBtn);

    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setFixedWidth(45);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    toolbar->addWidget(m_zoomLabel);

    QToolButton *zoomInBtn = new QToolButton(this);
    zoomInBtn->setText("+");
    zoomInBtn->setToolTip(tr("Zoom In (Ctrl++)"));
    connect(zoomInBtn, &QToolButton::clicked, this, &MainWindow::zoomIn);
    toolbar->addWidget(zoomInBtn);

    toolbar->addSeparator();

    // Theme Toggle Button
    m_themeButton = new QToolButton(this);
    m_themeButton->setText(tr("🌓"));
    m_themeButton->setToolTip(tr("Toggle Dark / Light Theme (Ctrl+D)"));
    connect(m_themeButton, &QToolButton::clicked, this, &MainWindow::toggleTheme);
    toolbar->addWidget(m_themeButton);

    // Always on Top Button
    m_stayOnTopButton = new QToolButton(this);
    m_stayOnTopButton->setCheckable(true);
    m_stayOnTopButton->setIcon(QIcon(":/icons/pin.png"));
    m_stayOnTopButton->setToolTip(tr("Always on Top"));
    connect(m_stayOnTopButton, &QToolButton::toggled, this, &MainWindow::toggleStayOnTop);
    toolbar->addWidget(m_stayOnTopButton);

    // Preferences / Settings Button
    m_settingsButton = new QToolButton(this);
    m_settingsButton->setText("⚙");
    m_settingsButton->setToolTip(tr("Preferences & Settings"));
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::openSettings);
    toolbar->addWidget(m_settingsButton);

    // Spacer
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    // Status Indicator Label
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet("color: palette(mid); font-size: 11px; padding-right: 8px;");
    toolbar->addWidget(m_statusLabel);

    addToolBar(Qt::TopToolBarArea, toolbar);
}

void MainWindow::setupShortcuts()
{
    // Proper Exit shortcuts
    QShortcut *ctrlQ = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(ctrlQ, &QShortcut::activated, this, &MainWindow::exitApplication);

    QShortcut *altF4 = new QShortcut(QKeySequence("Alt+F4"), this);
    connect(altF4, &QShortcut::activated, this, &MainWindow::exitApplication);

    // New Chat
    QShortcut *ctrlN = new QShortcut(QKeySequence("Ctrl+N"), this);
    connect(ctrlN, &QShortcut::activated, this, [this]() {
        if (m_webPage) m_webPage->triggerNewChat();
    });

    // Search
    QShortcut *ctrlF = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(ctrlF, &QShortcut::activated, this, [this]() {
        if (m_webPage) m_webPage->triggerSearch();
    });

    // Chat navigation
    QShortcut *prevChat = new QShortcut(QKeySequence("Ctrl+Shift+["), this);
    connect(prevChat, &QShortcut::activated, this, [this]() {
        if (m_webPage) m_webPage->triggerPreviousChat();
    });

    QShortcut *nextChat = new QShortcut(QKeySequence("Ctrl+Shift+]"), this);
    connect(nextChat, &QShortcut::activated, this, [this]() {
        if (m_webPage) m_webPage->triggerNextChat();
    });

    // Zoom shortcuts
    QShortcut *zoomIn1 = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(zoomIn1, &QShortcut::activated, this, &MainWindow::zoomIn);

    QShortcut *zoomIn2 = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(zoomIn2, &QShortcut::activated, this, &MainWindow::zoomIn);

    QShortcut *zoomOutShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(zoomOutShortcut, &QShortcut::activated, this, &MainWindow::zoomOut);

    QShortcut *zoomReset = new QShortcut(QKeySequence("Ctrl+0"), this);
    connect(zoomReset, &QShortcut::activated, this, &MainWindow::resetZoom);

    // Reload shortcuts
    QShortcut *reloadF5 = new QShortcut(QKeySequence("F5"), this);
    connect(reloadF5, &QShortcut::activated, m_webView, &QWebEngineView::reload);

    QShortcut *reloadCtrlR = new QShortcut(QKeySequence("Ctrl+R"), this);
    connect(reloadCtrlR, &QShortcut::activated, m_webView, &QWebEngineView::reload);

    // Downloads
    QShortcut *ctrlJ = new QShortcut(QKeySequence("Ctrl+J"), this);
    connect(ctrlJ, &QShortcut::activated, this, &MainWindow::openDownloads);

    // Toggle Theme
    QShortcut *ctrlD = new QShortcut(QKeySequence("Ctrl+D"), this);
    connect(ctrlD, &QShortcut::activated, this, &MainWindow::toggleTheme);

    // Fullscreen F11
    QShortcut *f11 = new QShortcut(QKeySequence("F11"), this);
    connect(f11, &QShortcut::activated, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });
}

void MainWindow::setupMemoryOptimization()
{
    // Real-time memory profiling timer every 15 seconds
    m_memoryTimer = new QTimer(this);
    m_memoryTimer->setInterval(15000); // 15 seconds
    connect(m_memoryTimer, &QTimer::timeout, this, &MainWindow::logMemoryUsage);
    m_memoryTimer->start();

    // Aggressive glibc heap arena trimming & V8 GC every 30 seconds
    QTimer *trimTimer = new QTimer(this);
    trimTimer->setInterval(30000); // 30 seconds
    connect(trimTimer, &QTimer::timeout, this, &MainWindow::optimizeMemoryUsage);
    trimTimer->start();

    // Log initial memory on startup
    QTimer::singleShot(3000, this, &MainWindow::logMemoryUsage);
}

void MainWindow::logMemoryUsage()
{
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly)) {
        double vmrssMb = 0.0;
        double rssAnonMb = 0.0;
        double rssFileMb = 0.0;
        double rssShmemMb = 0.0;

        const QByteArray content = statusFile.readAll();
        const QList<QByteArray> lines = content.split('\n');
        for (const QByteArray &rawLine : lines) {
            QString line = QString::fromLatin1(rawLine);
            if (line.startsWith("VmRSS:")) {
                vmrssMb = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(1).toDouble() / 1024.0;
            } else if (line.startsWith("RssAnon:")) {
                rssAnonMb = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(1).toDouble() / 1024.0;
            } else if (line.startsWith("RssFile:")) {
                rssFileMb = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(1).toDouble() / 1024.0;
            } else if (line.startsWith("RssShmem:")) {
                rssShmemMb = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(1).toDouble() / 1024.0;
            }
        }

        qDebug().noquote() << QString("[Memory Profile] Private RAM: %1 MB (Anon) | Shared Libs: %2 MB (File) | Total RSS: %3 MB")
                                  .arg(rssAnonMb, 0, 'f', 2)
                                  .arg(rssFileMb + rssShmemMb, 0, 'f', 2)
                                  .arg(vmrssMb, 0, 'f', 2);
    }

    if (m_webPage) {
        m_webPage->runJavaScript(
            "if (window.performance && window.performance.memory) {"
            "  console.log('[V8 Memory] Used Heap: ' + (window.performance.memory.usedJSHeapSize / 1048576).toFixed(2) + ' MB / ' +"
            "              'Total Heap: ' + (window.performance.memory.totalJSHeapSize / 1048576).toFixed(2) + ' MB');"
            "}"
        );
    }
}

void MainWindow::optimizeMemoryUsage()
{
    // 1. Run JavaScript V8 Garbage Collector
    if (m_webPage) {
        m_webPage->runJavaScript("if (window.gc) window.gc();");
    }

    // 2. Return glibc heap arenas back to OS kernel immediately
    malloc_trim(0);

    qDebug() << "Aggressive memory optimization & glibc malloc_trim(0) executed";
}

void MainWindow::handleTitleChanged(const QString &title)
{
    // Parse unread count from title: e.g. "(3) WhatsApp"
    if (title.contains('(')) {
        QString countStr = title.section('(', 1, 1).section(')', 0, 0);
        bool ok = false;
        int count = countStr.toInt(&ok);
        if (ok) {
            m_trayManager->setUnreadCount(count);
            setWindowTitle(QString("(%1) WhatsApp Desktop").arg(count));
            return;
        }
    }

    m_trayManager->setUnreadCount(0);
    setWindowTitle("WhatsApp Desktop");
}

void MainWindow::handleIncomingCall(const QString &callerName, bool isVideo, const QString &avatarPath)
{
    // Debounce duplicate incoming call notifications (25s window per caller)
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (callerName == m_lastCallCaller && (now - m_lastCallTime) < 25000) {
        qDebug() << "Debouncing duplicate incoming call notification for:" << callerName;
        return;
    }
    m_lastCallCaller = callerName;
    m_lastCallTime = now;

    qDebug() << "Incoming WhatsApp call detected from:" << callerName << "Video:" << isVideo << "Avatar:" << avatarPath;

    // Real WhatsApp format:
    // Notification Title / Summary: Caller Name (e.g. "John Doe")
    // Notification Body / Message: "Incoming voice call" or "Incoming video call"
    QString callType = isVideo ? tr("Incoming video call") : tr("Incoming voice call");
    QString iconToUse = (!avatarPath.isEmpty() && QFile::exists(avatarPath))
                        ? avatarPath
                        : (isVideo ? "camera-web" : "call-start");

    m_systemIntegration->showNativeNotification(
        callerName,
        callType,
        iconToUse,
        true
    );

    // Request desktop attention and window urgency alert
    m_systemIntegration->requestAttention(this);
}

void MainWindow::toggleVisibility()
{
    if (isVisible() && !isMinimized()) {
        // Save window geometry before hiding
        QSettings settings;
        settings.setValue("Geometry/MainWindow", saveGeometry());

        if (m_systemIntegration->isSystemTrayAvailable()) {
            hide();
        } else {
            setWindowState(windowState() | Qt::WindowMinimized);
        }
        // Immediately release memory arenas and run GC on hide/minimize
        optimizeMemoryUsage();
    } else {
        if (isMinimized()) {
            setWindowState(windowState() & ~Qt::WindowMinimized);
        }
        show();
        showNormal();
        activateWindow();
        raise();
        m_systemIntegration->requestAttention(this);
    }
}

void MainWindow::exitApplication()
{
    qDebug() << "Exit requested: Performing clean shutdown...";
    m_forceClose = true;

    // Save geometry on exit
    QSettings settings;
    settings.setValue("Geometry/MainWindow", saveGeometry());

    if (m_memoryTimer) {
        m_memoryTimer->stop();
    }

    if (m_trayManager && m_trayManager->icon()) {
        m_trayManager->icon()->hide();
    }

    close();
    QTimer::singleShot(100, qApp, &QApplication::quit);
}

void MainWindow::openSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(m_downloadManager, this);
        connect(m_settingsDialog, &SettingsDialog::themeChanged, this, [this](int mode) {
            if (mode == 1) applyTheme(true);
            else if (mode == 2) applyTheme(false);
            else applyTheme(m_systemIntegration->isSystemDarkTheme());
        });
        connect(m_settingsDialog, &SettingsDialog::zoomChanged, this, [this](qreal zoom) {
            m_currentZoom = zoom;
            m_webView->setZoomFactor(zoom);
            updateZoomLabel();
        });
    }
    m_settingsDialog->exec();
}

void MainWindow::openDownloads()
{
    if (m_downloadManager) {
        m_downloadManager->showDownloadsDialog(this);
    }
}

void MainWindow::toggleTheme()
{
    applyTheme(!m_isDarkTheme);
}

void MainWindow::applyTheme(bool dark)
{
    m_isDarkTheme = dark;
    if (m_webPage) {
        m_webPage->applyTheme(dark);
    }
}

void MainWindow::zoomIn()
{
    m_currentZoom = qMin(2.0, m_currentZoom + 0.1);
    m_webView->setZoomFactor(m_currentZoom);
    updateZoomLabel();
}

void MainWindow::zoomOut()
{
    m_currentZoom = qMax(0.5, m_currentZoom - 0.1);
    m_webView->setZoomFactor(m_currentZoom);
    updateZoomLabel();
}

void MainWindow::resetZoom()
{
    m_currentZoom = 1.0;
    m_webView->setZoomFactor(m_currentZoom);
    updateZoomLabel();
}

void MainWindow::updateZoomLabel()
{
    if (m_zoomLabel) {
        m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_currentZoom * 100)));
    }
}

void MainWindow::toggleStayOnTop(bool checked)
{
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
}

void MainWindow::onExportTriggered()
{
    if (m_exportDialog) {
        m_exportDialog->exportChatToCSV();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue("Geometry/MainWindow", saveGeometry());

    if (m_forceClose) {
        event->accept();
        return;
    }

    if (SettingsDialog::getCloseToTray()) {
        if (m_systemIntegration->isSystemTrayAvailable()) {
            hide();
            event->ignore();
            return;
        }
    }

    // Default exit if close to tray is disabled
    event->accept();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
}
