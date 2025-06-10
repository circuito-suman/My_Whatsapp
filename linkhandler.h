#ifndef LINKHANDLER_H
#define LINKHANDLER_H

#include <QWebEnginePage>
#include <QMenu>
#include <QToolTip>
#include <QContextMenuEvent>
#include <QWebEngineView>
#include<QWebEngineUrlRequestInterceptor>
#include<QWebEngineUrlRequestInfo>
#include<QDesktopServices>
class LinkHandler : public QWebEnginePage {
    Q_OBJECT
public:
    explicit LinkHandler(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
    void externalLinkRequested(const QUrl &url);
    void internalLinkRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;

private slots:
    void handleLinkHover(const QString &url);
    void contextMenuEvent(QContextMenuEvent *event);


private:
    QMenu *linkContextMenu;
    QUrl hoveredUrl;
    QWebEngineView *webView;

};


class PolicyInterceptor : public QWebEngineUrlRequestInterceptor {
public:
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        // Remove conflicting headers

    }
};



#endif // LINKHANDLER_H
