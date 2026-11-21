#include "systemintegration.h"
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QWindow>
#include <QDebug>

#ifdef HAVE_DBUS
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#endif

SystemIntegration::SystemIntegration(QObject *parent)
    : QObject(parent)
    , m_trayAvailabilityChecked(false)
    , m_trayAvailable(false)
{
    m_desktopEnv = detectDesktopEnvironment();
    m_displayServer = detectDisplayServer();

    qDebug() << "Detected desktop environment:" << getDesktopEnvironmentName();
    qDebug() << "Display server:" << (m_displayServer == Wayland ? "Wayland" :
                                     m_displayServer == X11 ? "X11" : "Unknown");

    // Monitor system color scheme (Dark / Light)
    if (QGuiApplication::styleHints()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
            emit systemThemeChanged(scheme == Qt::ColorScheme::Dark);
        });
#endif
    }

#ifdef HAVE_DBUS
    // Connect to notification action callbacks
    QDBusConnection::sessionBus().connect(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "ActionInvoked",
        this,
        SLOT(onDBusNotificationAction(uint, QString))
    );
#endif
}

SystemIntegration::DesktopEnvironment SystemIntegration::detectDesktopEnvironment() const
{
    QString currentDesktop = getEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();

    if (currentDesktop.contains("gnome")) return GNOME;
    if (currentDesktop.contains("kde")) return KDE;
    if (currentDesktop.contains("xfce")) return XFCE;
    if (currentDesktop.contains("mate")) return MATE;
    if (currentDesktop.contains("cinnamon")) return Cinnamon;
    if (currentDesktop.contains("unity")) return Unity;
    if (currentDesktop.contains("lxde")) return LXDE;
    if (currentDesktop.contains("lxqt")) return LXQt;
    if (currentDesktop.contains("budgie")) return Budgie;
    if (currentDesktop.contains("sway")) return Sway;
    if (currentDesktop.contains("hyprland")) return Hyprland;

    QString desktopSession = getEnvironmentVariable("DESKTOP_SESSION").toLower();
    if (desktopSession.contains("gnome")) return GNOME;
    if (desktopSession.contains("kde")) return KDE;
    if (desktopSession.contains("xfce")) return XFCE;

    if (!getEnvironmentVariable("GNOME_DESKTOP_SESSION_ID").isEmpty()) return GNOME;
    if (!getEnvironmentVariable("KDE_FULL_SESSION").isEmpty()) return KDE;

    if (isProcessRunning("gnome-shell")) return GNOME;
    if (isProcessRunning("kwin")) return KDE;
    if (isProcessRunning("xfwm4")) return XFCE;
    if (isProcessRunning("sway")) return Sway;
    if (isProcessRunning("Hyprland")) return Hyprland;

    return Unknown;
}

SystemIntegration::DisplayServer SystemIntegration::detectDisplayServer() const
{
    QString waylandDisplay = getEnvironmentVariable("WAYLAND_DISPLAY");
    QString x11Display = getEnvironmentVariable("DISPLAY");

    if (!waylandDisplay.isEmpty()) return Wayland;
    if (!x11Display.isEmpty()) return X11;

    return UnknownServer;
}

QString SystemIntegration::getDesktopEnvironmentName() const
{
    switch (m_desktopEnv) {
        case GNOME: return "GNOME";
        case KDE: return "KDE";
        case XFCE: return "XFCE";
        case MATE: return "MATE";
        case Cinnamon: return "Cinnamon";
        case Unity: return "Unity";
        case LXDE: return "LXDE";
        case LXQt: return "LXQt";
        case Budgie: return "Budgie";
        case Sway: return "Sway";
        case Hyprland: return "Hyprland";
        default: return "Unknown";
    }
}

bool SystemIntegration::isSystemTrayAvailable() const
{
    if (!m_trayAvailabilityChecked) {
        m_trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
        m_trayAvailabilityChecked = true;
    }
    return m_trayAvailable;
}

bool SystemIntegration::isSystemDarkTheme() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::WindowText).lightness() > palette.color(QPalette::Window).lightness();
}

bool SystemIntegration::showNativeNotification(const QString &title, const QString &message,
                                               const QString &icon, bool isCall) const
{
#ifdef HAVE_DBUS
    if (supportsNativeNotifications()) {
        return sendDBusNotification(title, message, icon, isCall);
    }
#endif

    QStringList args;
    QString command = "notify-send";
    args << title << message;
    if (!icon.isEmpty()) {
        args << "--icon" << icon;
        if (QFile::exists(icon)) {
            args << "-h" << QString("string:image-path:%1").arg(icon);
        }
    }
    if (isCall) {
        args << "--urgency=critical";
    }

    QProcess process;
    process.start(command, args);
    process.waitForFinished(3000);
    return process.exitCode() == 0;
}

bool SystemIntegration::setUrgencyHint(QWidget *window, bool urgent) const
{
    if (urgent && window) {
        window->activateWindow();
        window->raise();
        if (window->windowHandle()) {
            window->windowHandle()->alert(5000);
        }
        return true;
    }
    return false;
}

bool SystemIntegration::requestAttention(QWidget *window) const
{
    if (!window) return false;
    window->activateWindow();
    window->raise();
    if (window->windowHandle()) {
        window->windowHandle()->alert(5000);
    }
    return setUrgencyHint(window, true);
}

void SystemIntegration::setWindowManagerHints(QWidget *window) const
{
    if (!window) return;
    if (m_desktopEnv == KDE) {
        window->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop, false);
    }
}

void SystemIntegration::optimizeForEnvironment(QWidget *window) const
{
    if (!window) return;
    setWindowManagerHints(window);
}

bool SystemIntegration::supportsNativeNotifications() const
{
#ifdef HAVE_DBUS
    return QDBusConnection::sessionBus().isConnected();
#else
    return false;
#endif
}

#ifdef HAVE_DBUS
bool SystemIntegration::sendDBusNotification(const QString &title, const QString &message,
                                            const QString &icon, bool isCall) const
{
    QDBusInterface interface("org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications",
                           QDBusConnection::sessionBus());

    if (!interface.isValid()) {
        return false;
    }

    QString appIcon = icon.isEmpty() ? "whatsapp-desktop" : icon;

    QVariantMap hints;
    // Urgency: 0 = Low, 1 = Normal, 2 = Critical
    hints["urgency"] = QVariant::fromValue(static_cast<uchar>(isCall ? 2 : 1));
    hints["desktop-entry"] = "whatsapp-desktop";
    hints["category"] = isCall ? "call" : "im.received";

    if (!icon.isEmpty() && QFile::exists(icon)) {
        hints["image-path"] = icon;
        hints["image_path"] = icon;
        appIcon = icon;
    }

    QStringList actions;
    actions << "default" << tr("View");

    QList<QVariant> args;
    args << "WhatsApp Desktop";                  // app_name
    args << static_cast<uint>(0);                // replaces_id
    args << appIcon;                             // app_icon
    args << title;                               // summary
    args << message;                             // body
    args << actions;                             // actions
    args << hints;                               // hints
    args << static_cast<int>(isCall ? 15000 : 5000); // timeout (ms)

    QDBusReply<uint> reply = interface.callWithArgumentList(QDBus::AutoDetect, "Notify", args);
    return reply.isValid();
}

bool SystemIntegration::setDBusUrgencyHint(QWidget *window, bool urgent) const
{
    Q_UNUSED(window)
    Q_UNUSED(urgent)
    return false;
}

void SystemIntegration::onDBusNotificationAction(uint id, const QString &actionKey)
{
    Q_UNUSED(id)
    emit notificationActionInvoked(actionKey);
}
#endif

QString SystemIntegration::getEnvironmentVariable(const QString &name) const
{
    return QString::fromLocal8Bit(qgetenv(name.toLocal8Bit()));
}

bool SystemIntegration::isProcessRunning(const QString &processName) const
{
    QProcess process;
    process.start("pgrep", QStringList() << processName);
    process.waitForFinished(3000);
    return process.exitCode() == 0;
}
