#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

class DownloadManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(DownloadManager *downloadManager, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    static QString getDownloadDirectory();
    static bool getAutoDownload();
    static bool getStartMinimized();
    static bool getCloseToTray();
    static int getThemeMode(); // 0 = System, 1 = Dark, 2 = Light
    static qreal getZoomFactor();
    static bool getNotificationsEnabled();
    static bool getShowNotificationPreview();

signals:
    void themeChanged(int themeMode);
    void zoomChanged(qreal zoomFactor);
    void downloadSettingsChanged();

private slots:
    void onBrowseDownloadDir();
    void onSaveClicked();
    void onCancelClicked();

private:
    void loadSettings();
    void saveSettings();

    DownloadManager *m_downloadManager;
    QLineEdit *m_downloadDirEdit;
    QCheckBox *m_autoDownloadCheck;
    QCheckBox *m_startMinimizedCheck;
    QCheckBox *m_closeToTrayCheck;
    QComboBox *m_themeCombo;
    QComboBox *m_zoomCombo;
    QCheckBox *m_notificationsCheck;
    QCheckBox *m_notificationPreviewCheck;
};

#endif // SETTINGSDIALOG_H
