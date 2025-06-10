#ifndef MYWHATSAPPWINDOW_H
#define MYWHATSAPPWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QSystemTrayIcon>
#include <QWebEnginePage>
#include <QListWidget>
#include <QSplitter>        // Critical for QSplitter
#include <QJsonDocument>    // For JSON handling
#include <QJsonArray>       // For JSON arrays
#include <QDesktopServices> // For opening URLs
#include <QVBoxLayout>      // For vertical layouts
#include <QPushButton>      // For buttons
#include <QJsonDocument>    // Already in header, but safe to include here
#include <QJsonArray>       // Already in header
#include <QJsonObject>      // Add this line
#include <QDesktopServices> // Already in header
#include <QDockWidget>
#include <QToolBar>
#include <QTimer>
#include <QToolButton>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QPrintPreviewDialog>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QWebEngineNavigationRequest> // Add this at the top

#include"linkhandler.h"

namespace Ui
{
    class MyWhatsappWindow;
}

class MyWhatsappWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MyWhatsappWindow(QWidget *parent = nullptr);
    QHash<QString, QSystemTrayIcon::MessageIcon> activeNotifications;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleVisibility();
    void updateUnreadCount(const QString &title);
    void refreshMediaGallery();
    void onMediaItemClicked(QListWidgetItem *item);
    void exportChatToCSV();
    void toggleStayOnTop(bool checked);
    bool shouldShowNotification(const QString &title, const QString &message);
    void showPdfViewer(const QUrl &pdfUrl);
    bool currentUrlIsPdf();
    void showPrintPreview();
    void clearStaleNotifications();
    void onMessageClicked();
    void onRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode);

private:
    Ui::MyWhatsappWindow *ui; // Keep this
    QToolButton *stayOnTopButton;

    QWebEngineView *webView;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QSplitter *splitter;
    QDockWidget *mediaDock;
    QListWidget *mediaList;
    QAction *toggleMediaAction;
    QToolButton *exportButton;
    void notify(const QString &message);

};

#endif // MYWHATSAPPWINDOW_H
