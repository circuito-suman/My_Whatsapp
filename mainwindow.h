#ifndef MYWHATSAPPWINDOW_H
#define MYWHATSAPPWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>

class MyWhatsappWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MyWhatsappWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLoadFinished(bool ok);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showWindow();
    void minimizeToTray();
    void toggleFullScreen();

private:
    QWebEngineView *webView;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QAction *restoreAction;
    QAction *minimizeAction;
    QAction *quitAction;
};

#endif // MYWHATSAPPWINDOW_H

