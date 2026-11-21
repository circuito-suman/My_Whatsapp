#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QToolButton>
#include <QLabel>
#include <QTimer>
#include <QShortcut>

class WebPage;
class TrayManager;
class MediaGallery;
class ExportDialog;
class SystemIntegration;
class DownloadManager;
class SettingsDialog;
class MprisPlayer;
class MediaSchemeHandler;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:
    void toggleVisibility();
    void exitApplication();
    void openSettings();
    void openDownloads();
    void toggleTheme();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void applyTheme(bool dark);

private slots:
    void handleTitleChanged(const QString &title);
    void handleIncomingCall(const QString &callerName, bool isVideo, const QString &avatarPath = QString());
    void optimizeMemoryUsage();
    void logMemoryUsage();
    void toggleStayOnTop(bool checked);
    void onExportTriggered();

private:
    void setupUI();
    void setupProfile();
    void setupShortcuts();
    void setupMemoryOptimization();
    void updateZoomLabel();

    QWebEngineProfile *m_profile;
    QWebEngineView *m_webView;
    WebPage *m_webPage;
    TrayManager *m_trayManager;
    MediaGallery *m_mediaGallery;
    ExportDialog *m_exportDialog;
    SystemIntegration *m_systemIntegration;
    DownloadManager *m_downloadManager;
    SettingsDialog *m_settingsDialog;
    MprisPlayer *m_mprisPlayer;
    MediaSchemeHandler *m_mediaSchemeHandler;

    QString m_lastDetectedCallerName;
    QString m_lastCallCaller;
    qint64 m_lastCallTime = 0;

    QToolButton *m_stayOnTopButton;
    QToolButton *m_themeButton;
    QToolButton *m_downloadsButton;
    QToolButton *m_settingsButton;
    QLabel *m_zoomLabel;
    QLabel *m_statusLabel;

    QTimer *m_memoryTimer;
    bool m_forceClose;
    bool m_isDarkTheme;
    qreal m_currentZoom;
};

#endif // MAINWINDOW_H
