#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QAction>

class SystemIntegration;

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(SystemIntegration *systemIntegration, QObject *parent = nullptr);
    ~TrayManager() override;

    void setBaseIcon(const QIcon &icon);
    void setUnreadCount(int count);
    int unreadCount() const { return m_unreadCount; }

    bool isMuted() const { return m_isMuted; }
    void setMuted(bool muted);

    QSystemTrayIcon* icon() const;
    bool showMessage(const QString &title, const QString &message, const QString &icon = QString());

signals:
    void toggleVisibilityRequested();
    void exitRequested();
    void newChatRequested();
    void toggleThemeRequested();
    void openDownloadsRequested();
    void settingsRequested();
    void muteToggled(bool muted);

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void initializeTray();
    void updateTrayIcon();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_showHideAction;
    QAction *m_muteAction;
    QIcon m_baseIcon;
    int m_unreadCount;
    bool m_isMuted;
    SystemIntegration *m_systemIntegration;
};

#endif // TRAYMANAGER_H
