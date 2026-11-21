#ifndef MPRISPLAYER_H
#define MPRISPLAYER_H

#include <QObject>

#ifdef HAVE_DBUS
#include <QDBusConnection>
#include <QDBusAbstractAdaptor>
#include <QStringList>

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MprisPlayerAdaptor(QObject *parent);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "WhatsApp Desktop"; }
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise();
    void Quit();

signals:
    void raiseRequested();
    void quitRequested();
};

class MprisPlayerControlAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY PlaybackStatusChanged)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)

public:
    explicit MprisPlayerControlAdaptor(QObject *parent);

    QString playbackStatus() const { return m_status; }
    bool canControl() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return false; }

    void setStatus(const QString &status) {
        if (m_status != status) {
            m_status = status;
            emit PlaybackStatusChanged(m_status);
        }
    }

public slots:
    void PlayPause();
    void Play();
    void Pause();
    void Stop();

signals:
    void playPauseRequested();
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void PlaybackStatusChanged(const QString &status);

private:
    QString m_status = "Stopped";
};
#endif // HAVE_DBUS

class MprisPlayer : public QObject
{
    Q_OBJECT
public:
    explicit MprisPlayer(QObject *parent = nullptr);
    ~MprisPlayer() override;

    void setPlaybackStatus(const QString &status);

signals:
    void playPauseRequested();
    void raiseRequested();
    void quitRequested();

private:
#ifdef HAVE_DBUS
    MprisPlayerAdaptor *m_rootAdaptor;
    MprisPlayerControlAdaptor *m_playerAdaptor;
#endif
};

#endif // MPRISPLAYER_H
