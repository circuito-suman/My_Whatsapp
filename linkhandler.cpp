#include "linkhandler.h"


// LinkHandler::LinkHandler(QObject *parent)
//     : QWebEnginePage(parent) {
//     // Enable link hover tracking
//     connect(this, &QWebEnginePage::linkHovered,
//             this, &LinkHandler::handleLinkHover);
// }



LinkHandler::LinkHandler(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent) {
        connect(this, &QWebEnginePage::linkHovered,
                this, &LinkHandler::handleLinkHover);
    // Constructor uses the provided profile
}

// linkhandler.cpp
bool LinkHandler::acceptNavigationRequest(const QUrl &url,
                                          NavigationType type,
                                          bool isMainFrame) {
    qDebug() << "Navigation request:" << url << "Type:" << type;

    if (type == NavigationTypeLinkClicked) {
        // Check if URL is external
        if (url.host() != "web.whatsapp.com") {
            QDesktopServices::openUrl(url);
            return false; // Block navigation in app
        }
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}



void LinkHandler::handleLinkHover(const QString &url) {
    hoveredUrl = QUrl(url);
    // Show custom tooltip with open options
    QToolTip::showText(QCursor::pos(),
                       QString("Open in:\n- Browser (Ctrl+Click)\n- App (Shift+Click)"));
}



void LinkHandler::contextMenuEvent(QContextMenuEvent *event) {
    linkContextMenu = new QMenu;

    QAction *externalAction = linkContextMenu->addAction("Open in Browser");
    connect(externalAction, &QAction::triggered, [this]() {
        emit externalLinkRequested(hoveredUrl);
    });

    QAction *internalAction = linkContextMenu->addAction("Open in New Tab");
    connect(internalAction, &QAction::triggered, [this]() {
        emit internalLinkRequested(hoveredUrl);
    });

    linkContextMenu->exec(event->globalPos());
    delete linkContextMenu;
}


// Add to LinkHandler class
void LinkHandler::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                           const QString &message,
                                           int lineNumber,
                                           const QString &sourceID) {
    if(message.contains("AudioContext")) {
        webView->page()->runJavaScript(R"(
            document.body.addEventListener('click', () => {
                if(window.audioContext) return;
                window.audioContext = new (window.AudioContext || window.webkitAudioContext)();
                audioContext.resume();
            });
        )");
    }
}


