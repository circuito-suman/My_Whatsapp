#ifndef POPOUTWINDOW_H
#define POPOUTWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QUrl>

class WebPage;

class PopoutWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PopoutWindow(QWebEngineProfile *profile, QWidget *parent = nullptr);
    ~PopoutWindow() override;

    QWebEngineView *view() const { return m_view; }
    WebPage *page() const { return m_page; }

private slots:
    void handleTitleChanged(const QString &title);
    void handleUrlChanged(const QUrl &url);

private:
    QWebEngineView *m_view;
    WebPage *m_page;
};

#endif // POPOUTWINDOW_H
