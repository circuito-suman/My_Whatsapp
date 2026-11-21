#include <QApplication>
#include <QProcessEnvironment>
#include <QScreen>
#include <QDir>
#include <QStandardPaths>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <span>
#include <string_view>
#include <format>
#include "mainwindow.h"
#include "mediaschemehandler.h"

static const char *SINGLE_INSTANCE_SERVER = "whatsapp_desktop_instance_ipc";
static QFile s_logFile;

void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    const char *typeStr = "DEBUG";
    switch (type) {
    case QtDebugMsg:    typeStr = "DEBUG"; break;
    case QtInfoMsg:     typeStr = "INFO "; break;
    case QtWarningMsg:  typeStr = "WARN "; break;
    case QtCriticalMsg: typeStr = "CRIT "; break;
    case QtFatalMsg:    typeStr = "FATAL"; break;
    }

    QString formattedMsg = QString("[%1] [%2] %3\n").arg(timestamp, typeStr, msg);

    fprintf(stderr, "%s", formattedMsg.toLocal8Bit().constData());
    fflush(stderr);

    if (s_logFile.isOpen()) {
        s_logFile.write(formattedMsg.toUtf8());
        s_logFile.flush();
    }
}

void setupEnvironmentFlags() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString waylandDisplay = env.value("WAYLAND_DISPLAY");
    QString sessionType = env.value("XDG_SESSION_TYPE");

    QString chromiumFlags;

    // Ultra-low RAM & V8 Footprint Minimization (<200MB target)
    // Removed --enable-low-end-device-mode to prevent Blink viewport hopping / layout resizing
    chromiumFlags += " --max-old-space-size=256";
    chromiumFlags += " --optimize-for-size";
    chromiumFlags += " --renderer-process-limit=1";
    chromiumFlags += " --js-flags=--expose-gc";

    // Security & Network Leak Prevention
    // Disable web security to allow local custom scheme (wa-media://) media offloading past WhatsApp CSP
    chromiumFlags += " --disable-web-security";
    chromiumFlags += " --disable-extensions";
    chromiumFlags += " --disable-plugins";
    chromiumFlags += " --disable-background-mode";
    chromiumFlags += " --disable-breakpad";
    chromiumFlags += " --disable-client-side-phishing-detection";
    chromiumFlags += " --disable-component-update";
    chromiumFlags += " --disable-domain-reliability";
    chromiumFlags += " --disable-sync";
    chromiumFlags += " --no-default-browser-check";
    chromiumFlags += " --enable-strict-mixed-content-checking";
    chromiumFlags += " --enable-strict-powerful-feature-restrictions";

    // WebRTC & Calling (Passthrough command decoder, hardware VA-API video decode, autoplay)
    // Note: --use-cmd-decoder=passthrough ensures caps.supports_yuv_to_rgb_conversion = true.
    chromiumFlags += " --use-cmd-decoder=passthrough";
    chromiumFlags += " --enable-features=VaapiVideoDecoder,VaapiVideoEncoder,AcceleratedVideoDecodeLinuxGL,PlatformHEVCDecoderSupport,WebRTCPipeWireCapturer";
    chromiumFlags += " --disable-features=IsolateOrigins,site-per-process,Translate,OptimizationHints,MediaRouter";
    chromiumFlags += " --ignore-gpu-blocklist";
    chromiumFlags += " --enable-zero-copy";
    chromiumFlags += " --autoplay-policy=no-user-gesture-required";
    chromiumFlags += " --disable-background-media-suspend";
    chromiumFlags += " --log-level=3";

    // GPU & Video stability
    chromiumFlags += " --disable-gpu-process-crash-limit";

    // Compact Cache Limits (prevents ballooning memory)
    chromiumFlags += " --disk-cache-size=25165824";  // 24MB disk cache
    chromiumFlags += " --media-cache-size=16777216";  // 16MB media cache

    // Wayland & Desktop Media Capturer handling (PipeWire portal under Wayland)
    QString qtPlatform = env.value("QT_QPA_PLATFORM");
    bool isWaylandSession = (sessionType == "wayland") || !waylandDisplay.isEmpty() || (qtPlatform == "wayland");
    if (isWaylandSession) {
        qDebug() << "Detected Wayland session. Enabling Wayland Ozone platform and WebRTC PipeWire screen capturer.";
        chromiumFlags += " --ozone-platform-hint=auto";
        chromiumFlags += " --enable-wayland-ime";
    } else {
        if (!env.contains("DISPLAY") || env.value("DISPLAY").isEmpty()) {
            qputenv("DISPLAY", ":0");
        }
    }

    // Set Chromium Flags
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlags.toLocal8Bit());
    qunsetenv("QTWEBENGINE_REMOTE_DEBUGGING");
    qputenv("QT_LOGGING_RULES", "qt.webenginecontext.debug=false");

    // License check bypass
    qputenv("QTFRAMEWORK_BYPASS_LICENSE_CHECK", "1");
}

void setupApplicationAttributes() {
    // Fixed DPI scaling to avoid oversized UI elements or cursors
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
    qputenv("QT_SCALE_FACTOR", "1.0");
    qputenv("QT_SCREEN_SCALE_FACTORS", "1.0");
    qputenv("QT_FONT_DPI", "96");
    qputenv("QT_DEVICE_PIXEL_RATIO", "1");
    qputenv("XCURSOR_SIZE", "24");
    qputenv("XCURSOR_THEME", "default");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString desktop = env.value("XDG_CURRENT_DESKTOP").toLower();
    if (desktop.contains("gnome") || desktop.contains("unity")) {
        qputenv("GDK_SCALE", "1");
        qputenv("GDK_DPI_SCALE", "1");
    } else if (desktop.contains("kde")) {
        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "Round");
    }
}

int main(int argc, char *argv[]) {
    // C++20 std::span for bounds-safe argument processing
    std::span<char*> cliArgs(argv, static_cast<std::size_t>(argc));
    for (std::string_view arg : cliArgs) {
        if (arg == "--version" || arg == "-v") {
            fprintf(stdout, "WhatsApp Desktop 1.1.0 (C++20 Native Linux)\n");
            return 0;
        }
    }
    // Initialize logging to runtime_debug.log
    QString logPath = QDir::currentPath() + "/runtime_debug.log";
    s_logFile.setFileName(logPath);
    s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(customLogHandler);

    qDebug() << "================== WhatsApp Desktop Session Initialized ==================";
    qDebug() << "Logging active at:" << logPath;

    setupEnvironmentFlags();
    setupApplicationAttributes();

    // Register custom URL scheme before QApplication initialization
    MediaSchemeHandler::registerScheme();

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/whatsapp.png"));
    QCoreApplication::setOrganizationName("circuito_suman");
    QCoreApplication::setApplicationName("WhatsApp Desktop");
    QCoreApplication::setApplicationVersion("1.1.0");

    // Single Instance Guard via QLocalSocket
    QLocalSocket socket;
    socket.connectToServer(SINGLE_INSTANCE_SERVER);
    if (socket.waitForConnected(500)) {
        qDebug() << "Another instance of WhatsApp Desktop is already running. Raising it...";
        socket.write("SHOW\n");
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0; // Gracefully exit secondary instance
    }

    // Create Local Server for primary instance
    QLocalServer server;
    // Remove stale socket file if left over from crash
    QLocalServer::removeServer(SINGLE_INSTANCE_SERVER);
    if (!server.listen(SINGLE_INSTANCE_SERVER)) {
        qWarning() << "Could not start single instance IPC server:" << server.errorString();
    }

    // Ensure application data directory exists with restricted permissions (0700)
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    QFile::setPermissions(appDataPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    // Initialize Main Window (Window handles its own initial sizing and geometry restoration)
    MainWindow window;

    // Listen for wake-up requests from secondary instances
    QObject::connect(&server, &QLocalServer::newConnection, [&window, &server]() {
        QLocalSocket *client = server.nextPendingConnection();
        if (client) {
            client->waitForReadyRead(500);
            window.toggleVisibility();
            client->deleteLater();
        }
    });

    window.show();
    return app.exec();
}
