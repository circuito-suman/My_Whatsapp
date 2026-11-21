#include "traymanager.h"
#include "systemintegration.h"
#include <numbers>
#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QTimer>
#include <QDebug>

namespace {
// C++20 struct with designated initializers for badge layout
struct BadgeMetrics {
    int height = 48;
    int horizontalPadding = 18;
    int fontSize = 36;
    int margin = 2;
    int borderWidth = 3;
};
}

TrayManager::TrayManager(SystemIntegration *systemIntegration, QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_trayMenu(new QMenu())
    , m_baseIcon(QIcon(":/icons/whatsapp.png"))
    , m_unreadCount(0)
    , m_isMuted(false)
    , m_systemIntegration(systemIntegration)
{
    m_trayIcon->setIcon(m_baseIcon);

    m_showHideAction = new QAction(tr("Open WhatsApp"), this);
    QFont boldFont = m_showHideAction->font();
    boldFont.setBold(true);
    m_showHideAction->setFont(boldFont);
    connect(m_showHideAction, &QAction::triggered, this, &TrayManager::toggleVisibilityRequested);
    m_trayMenu->addAction(m_showHideAction);

    m_trayMenu->addSeparator();

    QAction *newChatAction = new QAction(tr("New Chat"), this);
    connect(newChatAction, &QAction::triggered, this, &TrayManager::newChatRequested);
    m_trayMenu->addAction(newChatAction);

    QAction *toggleThemeAction = new QAction(tr("Toggle Dark / Light Mode"), this);
    connect(toggleThemeAction, &QAction::triggered, this, &TrayManager::toggleThemeRequested);
    m_trayMenu->addAction(toggleThemeAction);

    QAction *openDownloadsAction = new QAction(tr("Open Downloads Folder"), this);
    connect(openDownloadsAction, &QAction::triggered, this, &TrayManager::openDownloadsRequested);
    m_trayMenu->addAction(openDownloadsAction);

    m_muteAction = new QAction(tr("Mute Notifications"), this);
    m_muteAction->setCheckable(true);
    connect(m_muteAction, &QAction::toggled, this, [this](bool checked) {
        m_isMuted = checked;
        emit muteToggled(checked);
    });
    m_trayMenu->addAction(m_muteAction);

    m_trayMenu->addSeparator();

    QAction *settingsAction = new QAction(tr("Preferences..."), this);
    connect(settingsAction, &QAction::triggered, this, &TrayManager::settingsRequested);
    m_trayMenu->addAction(settingsAction);

    m_trayMenu->addSeparator();

    QAction *quitAction = new QAction(tr("Quit WhatsApp"), this);
    connect(quitAction, &QAction::triggered, this, &TrayManager::exitRequested);
    m_trayMenu->addAction(quitAction);

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayManager::onTrayActivated);

    initializeTray();
}

TrayManager::~TrayManager()
{
    if (m_trayMenu) {
        delete m_trayMenu;
        m_trayMenu = nullptr;
    }
}

void TrayManager::setBaseIcon(const QIcon &icon)
{
    m_baseIcon = icon;
    updateTrayIcon();
}

void TrayManager::setUnreadCount(int count)
{
    if (m_unreadCount != count) {
        m_unreadCount = count;
        updateTrayIcon();
    }
}

void TrayManager::setMuted(bool muted)
{
    m_isMuted = muted;
    if (m_muteAction && m_muteAction->isChecked() != muted) {
        m_muteAction->setChecked(muted);
    }
}

QSystemTrayIcon* TrayManager::icon() const
{
    return m_trayIcon;
}

void TrayManager::updateTrayIcon()
{
    if (m_unreadCount <= 0) {
        m_trayIcon->setIcon(m_baseIcon);
        m_trayIcon->setToolTip(tr("WhatsApp Desktop"));
        return;
    }

    QPixmap basePixmap = m_baseIcon.pixmap(128, 128);
    if (basePixmap.isNull()) {
        basePixmap = QPixmap(":/icons/whatsapp.png").scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QPixmap badgedPixmap = basePixmap;
    QPainter painter(&badgedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // C++20 designated initializers for badge layout
    constexpr BadgeMetrics metrics{
        .height = 48,
        .horizontalPadding = 18,
        .fontSize = 36,
        .margin = 2,
        .borderWidth = 3
    };

    QString text = m_unreadCount > 99 ? "99+" : QString::number(m_unreadCount);

    QFont font = painter.font();
    font.setPixelSize(metrics.fontSize);
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);

    int badgeHeight = metrics.height;
    int badgeWidth = qMax(badgeHeight, textWidth + metrics.horizontalPadding);
    int x = badgedPixmap.width() - badgeWidth - metrics.margin;
    int y = metrics.margin;

    QRect badgeRect(x, y, badgeWidth, badgeHeight);

    // Outer border
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(badgeRect.adjusted(-metrics.borderWidth, -metrics.borderWidth, metrics.borderWidth, metrics.borderWidth),
                            badgeHeight / 2 + (metrics.borderWidth - 1),
                            badgeHeight / 2 + (metrics.borderWidth - 1));

    // Red badge background
    painter.setBrush(QColor("#E53935"));
    painter.drawRoundedRect(badgeRect, badgeHeight / 2, badgeHeight / 2);

    // Text
    painter.setPen(Qt::white);
    painter.drawText(badgeRect, Qt::AlignCenter, text);
    painter.end();

    m_trayIcon->setIcon(QIcon(badgedPixmap));
    m_trayIcon->setToolTip(tr("%1 unread messages - WhatsApp Desktop").arg(m_unreadCount));
}

void TrayManager::initializeTray()
{
    m_trayIcon->setVisible(true);

    // Verify visibility on desktop shells that take time to initialize
    QTimer::singleShot(1000, this, [this]() {
        if (!m_trayIcon->isVisible()) {
            m_trayIcon->setVisible(true);
        }
    });
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            emit toggleVisibilityRequested();
            break;
        default:
            break;
    }
}

bool TrayManager::showMessage(const QString &title, const QString &message, const QString &icon)
{
    if (m_isMuted) {
        return false;
    }

    if (m_systemIntegration && m_systemIntegration->showNativeNotification(title, message, icon)) {
        return true;
    }

    if (m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 5000);
        return true;
    }

    return false;
}
