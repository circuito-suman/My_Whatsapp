#include "popoutwindow.h"
#include "webpage.h"
#include <QDesktopServices>
#include <QIcon>
#include <QDebug>

PopoutWindow::PopoutWindow(QWebEngineProfile *profile, QWidget *parent)
    : QMainWindow(parent)
    , m_view(nullptr)
    , m_page(nullptr)
{
    setWindowTitle(tr("WhatsApp Call"));
    setWindowIcon(QIcon(":/icons/whatsapp.png"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 520);
    setMinimumSize(400, 300);

    m_view = new QWebEngineView(this);
    m_page = new WebPage(profile, m_view);
    m_view->setPage(m_page);
    setCentralWidget(m_view);

    connect(m_view, &QWebEngineView::titleChanged, this, &PopoutWindow::handleTitleChanged);
    connect(m_view, &QWebEngineView::urlChanged, this, &PopoutWindow::handleUrlChanged);
    connect(m_page, &QWebEnginePage::windowCloseRequested, this, [this]() {
        hide();
        close();
    });

    qDebug() << "PopoutWindow created for native WhatsApp popup/call hosting.";
}

PopoutWindow::~PopoutWindow()
{
    if (m_view) {
        delete m_view;
        m_view = nullptr;
        m_page = nullptr;
    }
    qDebug() << "PopoutWindow destroyed gracefully.";
}

void PopoutWindow::handleTitleChanged(const QString &title)
{
    if (!title.isEmpty()) {
        setWindowTitle(title);
    }
}

void PopoutWindow::handleUrlChanged(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.toString() == "about:blank") {
        return;
    }

    if (!WebPage::isTrustedHost(url.host())) {
        qDebug() << "PopoutWindow: External URL detected in popup. Offloading to system browser:" << url;
        QDesktopServices::openUrl(url);
        close();
    } else {
        qDebug() << "PopoutWindow: Displaying trusted WhatsApp popup window:" << url;
        if (!isVisible()) {
            show();
            raise();
            activateWindow();
        }
    }
}
