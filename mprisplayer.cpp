#include "mprisplayer.h"
#include <QDebug>

#ifdef HAVE_DBUS
MprisPlayerAdaptor::MprisPlayerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void MprisPlayerAdaptor::Raise()
{
    emit raiseRequested();
}

void MprisPlayerAdaptor::Quit()
{
    emit quitRequested();
}

MprisPlayerControlAdaptor::MprisPlayerControlAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void MprisPlayerControlAdaptor::PlayPause()
{
    emit playPauseRequested();
}

void MprisPlayerControlAdaptor::Play()
{
    emit playRequested();
}

void MprisPlayerControlAdaptor::Pause()
{
    emit pauseRequested();
}

void MprisPlayerControlAdaptor::Stop()
{
    emit stopRequested();
}
#endif

MprisPlayer::MprisPlayer(QObject *parent)
    : QObject(parent)
#ifdef HAVE_DBUS
    , m_rootAdaptor(nullptr)
    , m_playerAdaptor(nullptr)
#endif
{
#ifdef HAVE_DBUS
    m_rootAdaptor = new MprisPlayerAdaptor(this);
    m_playerAdaptor = new MprisPlayerControlAdaptor(this);

    connect(m_rootAdaptor, &MprisPlayerAdaptor::raiseRequested, this, &MprisPlayer::raiseRequested);
    connect(m_rootAdaptor, &MprisPlayerAdaptor::quitRequested, this, &MprisPlayer::quitRequested);
    connect(m_playerAdaptor, &MprisPlayerControlAdaptor::playPauseRequested, this, &MprisPlayer::playPauseRequested);
    connect(m_playerAdaptor, &MprisPlayerControlAdaptor::playRequested, this, &MprisPlayer::playPauseRequested);
    connect(m_playerAdaptor, &MprisPlayerControlAdaptor::pauseRequested, this, &MprisPlayer::playPauseRequested);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        const QString serviceName = "org.mpris.MediaPlayer2.whatsapp";
        if (bus.registerService(serviceName)) {
            bus.registerObject("/org/mpris/MediaPlayer2", this);
            qDebug() << "MPRIS2 DBus service registered:" << serviceName;
        } else {
            qDebug() << "Could not register MPRIS2 DBus service (may already be in use)";
        }
    }
#endif
}

MprisPlayer::~MprisPlayer()
{
#ifdef HAVE_DBUS
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.unregisterObject("/org/mpris/MediaPlayer2");
        bus.unregisterService("org.mpris.MediaPlayer2.whatsapp");
    }
#endif
}

void MprisPlayer::setPlaybackStatus(const QString &status)
{
#ifdef HAVE_DBUS
    if (m_playerAdaptor) {
        m_playerAdaptor->setStatus(status);
    }
#else
    Q_UNUSED(status)
#endif
}
