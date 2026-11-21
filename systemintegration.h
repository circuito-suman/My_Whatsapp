#ifndef SYSTEMINTEGRATION_H
#define SYSTEMINTEGRATION_H

#include <QObject>
#include <QString>
#include <QSystemTrayIcon>
#include <QWidget>

#ifdef HAVE_DBUS
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#endif

class SystemIntegration : public QObject {
    Q_OBJECT

public:
    enum DesktopEnvironment {
        Unknown,
        GNOME,
        KDE,
        XFCE,
        MATE,
        Cinnamon,
        Unity,
        LXDE,
        LXQt,
        Budgie,
        Sway,
        Hyprland
    };

    enum DisplayServer {
        X11,
        Wayland,
        UnknownServer
    };

    explicit SystemIntegration(QObject *parent = nullptr);

    DesktopEnvironment detectDesktopEnvironment() const;
    DisplayServer detectDisplayServer() const;
    QString getDesktopEnvironmentName() const;

    bool isSystemTrayAvailable() const;
    bool showNativeNotification(const QString &title, const QString &message,
                               const QString &icon = QString(), bool isCall = false) const;
    bool setUrgencyHint(QWidget *window, bool urgent = true) const;
    bool requestAttention(QWidget *window) const;

    void setWindowManagerHints(QWidget *window) const;
    void optimizeForEnvironment(QWidget *window) const;

    bool supportsNativeNotifications() const;
    bool isSystemDarkTheme() const;

signals:
    void systemThemeChanged(bool isDark);
    void notificationActionInvoked(const QString &action);

private slots:
    void onDBusNotificationAction(uint id, const QString &actionKey);

private:
    DesktopEnvironment m_desktopEnv;
    DisplayServer m_displayServer;
    mutable bool m_trayAvailabilityChecked;
    mutable bool m_trayAvailable;

#ifdef HAVE_DBUS
    bool sendDBusNotification(const QString &title, const QString &message,
                             const QString &icon = QString(), bool isCall = false) const;
    bool setDBusUrgencyHint(QWidget *window, bool urgent = true) const;
#endif

    QString getEnvironmentVariable(const QString &name) const;
    bool isProcessRunning(const QString &processName) const;
};

#endif // SYSTEMINTEGRATION_H
