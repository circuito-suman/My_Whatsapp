#include "mediaschemehandler.h"
#include "webpage.h"
#include <QWebEngineUrlScheme>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStandardPaths>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDebug>

void MediaSchemeHandler::registerScheme()
{
    QWebEngineUrlScheme mediaScheme("wa-media");
    mediaScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    QWebEngineUrlScheme::Flags flags = QWebEngineUrlScheme::SecureScheme |
                                       QWebEngineUrlScheme::CorsEnabled |
                                       QWebEngineUrlScheme::ContentSecurityPolicyIgnored;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    flags |= QWebEngineUrlScheme::FetchApiAllowed;
#endif
    mediaScheme.setFlags(flags);
    QWebEngineUrlScheme::registerScheme(mediaScheme);
    qDebug() << "MediaSchemeHandler: 'wa-media' scheme registered with SecureScheme, Host syntax, CORS, Fetch, and CSP-Ignored permissions";
}

MediaSchemeHandler::MediaSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
{
}

std::string_view MediaSchemeHandler::detectMimeType(std::span<const uint8_t> bytes)
{
    // C++20 bounds-safe span inspection for magic signatures
    if (bytes.size() >= 4) {
        // PNG magic: 0x89 'P' 'N' 'G'
        if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
            return "image/png";
        }
        // JPEG magic: 0xFF 0xD8 0xFF
        if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
            return "image/jpeg";
        }
    }
    if (bytes.size() >= 12) {
        // WebP magic: 'R' 'I' 'F' 'F' .... 'W' 'E' 'B' 'P'
        if (bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
            bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
            return "image/webp";
        }
    }
    if (bytes.size() >= 8) {
        // MP4 / ISO Base Media File: ....ftyp
        if (bytes[4] == 'f' && bytes[5] == 't' && bytes[6] == 'y' && bytes[7] == 'p') {
            return "video/mp4";
        }
    }
    return "application/octet-stream";
}

bool MediaSchemeHandler::playMediaFile(const QString &filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        qWarning() << "MediaSchemeHandler: Media file does not exist:" << filePath;
        return false;
    }

    QString mpv = QStandardPaths::findExecutable("mpv");
    if (!mpv.isEmpty()) {
        qDebug() << "MediaSchemeHandler: Spawning hardware-accelerated MPV for:" << filePath;
        return QProcess::startDetached(mpv, {
            "--title=WhatsApp Media",
            "--geometry=65%x65%",
            "--autofit=75%",
            "--force-window=immediate",
            "--keep-open=yes",
            filePath
        });
    }

    QString vlc = QStandardPaths::findExecutable("vlc");
    if (!vlc.isEmpty()) {
        qDebug() << "MediaSchemeHandler: Spawning VLC for:" << filePath;
        return QProcess::startDetached(vlc, {filePath});
    }

    qDebug() << "MediaSchemeHandler: Launching with system default media player:" << filePath;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MediaSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    const QUrl url = job->requestUrl();
    qDebug() << "MediaSchemeHandler: Received request:" << url.toString() << "Method:" << job->requestMethod();

    // Security origin check: Ensure requests are initiated only by trusted WhatsApp domains or custom scheme
    const QUrl initiator = job->initiator();
    if (initiator.isValid() && !initiator.host().isEmpty()) {
        if (!WebPage::isTrustedHost(initiator.host()) && initiator.scheme() != "wa-media") {
            qWarning() << "MediaSchemeHandler: Blocked unauthorized request from untrusted origin:" << initiator;
            job->fail(QWebEngineUrlRequestJob::RequestDenied);
            return;
        }
    }

    // Answer CORS preflight requests immediately with OK
    if (job->requestMethod() == "OPTIONS") {
        QBuffer *replyBuffer = new QBuffer(job);
        replyBuffer->open(QIODevice::ReadWrite);
        replyBuffer->write("OK");
        replyBuffer->seek(0);
        job->reply("text/plain", replyBuffer);
        return;
    }

    if (url.host() == "transcode" || url.path().contains("transcode")) {
        QByteArray data;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QIODevice *payload = job->requestBody();
        if (payload) {
            if (!payload->isOpen()) {
                payload->open(QIODevice::ReadOnly);
            }
            data = payload->readAll();
        }
#endif

        if (data.isEmpty()) {
            qWarning() << "MediaSchemeHandler: Empty payload for transcode request";
            job->fail(QWebEngineUrlRequestJob::RequestFailed);
            return;
        }

        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/transcoded_media";
        QDir().mkpath(tempDir);

        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
        QString cachedWebm = tempDir + "/" + hash + ".webm";

        if (!QFile::exists(cachedWebm) || QFileInfo(cachedWebm).size() == 0) {
            QString inPath = tempDir + "/" + hash + "_in.mp4";
            QFile inFile(inPath);
            if (inFile.open(QIODevice::WriteOnly)) {
                inFile.write(data);
                inFile.close();
            }

            // C++20 designated initializers for transcode settings
            MediaTranscodeOptions transcodeOpts{
                .videoBitrateKbps = 1500,
                .cpuUsed = 8,
                .hardwareAcceleration = true
            };

            qDebug() << "MediaSchemeHandler: Transcoding media (" << data.size() << "bytes, target bitrate:" << transcodeOpts.videoBitrateKbps << "kbps) to WebM...";

            // 1. Hardware accelerated VP9 encoding (Intel Iris Xe VA-API)
            int ret = -1;
            if (transcodeOpts.hardwareAcceleration) {
                ret = QProcess::execute("ffmpeg", {
                    "-y",
                    "-vaapi_device", "/dev/dri/renderD128",
                    "-i", inPath,
                    "-vf", "format=nv12,hwupload",
                    "-c:v", "vp9_vaapi",
                    "-b:v", QString::number(transcodeOpts.videoBitrateKbps) + "k",
                    "-c:a", "libopus",
                    cachedWebm
                });
            }

            // 2. Realtime VP8 software encoding fallback
            if (ret != 0 || !QFile::exists(cachedWebm) || QFileInfo(cachedWebm).size() == 0) {
                qDebug() << "MediaSchemeHandler: VA-API unavailable or failed, falling back to realtime software encoder";
                QProcess::execute("ffmpeg", {
                    "-y",
                    "-i", inPath,
                    "-c:v", "libvpx",
                    "-deadline", "realtime",
                    "-cpu-used", "8",
                    "-b:v", "1500k",
                    "-c:a", "libopus",
                    cachedWebm
                });
            }

            QFile::remove(inPath);
        }

        QFile *webmFile = new QFile(cachedWebm, job);
        if (webmFile->open(QIODevice::ReadOnly)) {
            qDebug() << "MediaSchemeHandler: Replying with transcoded WebM (" << webmFile->size() << "bytes)";
            job->reply("video/webm", webmFile);
        } else {
            qWarning() << "MediaSchemeHandler: Failed to open transcoded WebM file:" << cachedWebm;
            job->fail(QWebEngineUrlRequestJob::RequestFailed);
        }
        return;
    }

    if (url.host() == "play" || url.path().contains("play")) {
        QByteArray data;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QIODevice *payload = job->requestBody();
        if (payload) {
            if (!payload->isOpen()) {
                payload->open(QIODevice::ReadOnly);
            }
            data = payload->readAll();
        }
#endif

        if (!data.isEmpty()) {
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/temp_media";
            QDir().mkpath(tempDir);

            QString fileName = QString("WhatsApp_Video_%1.mp4").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
            QString filePath = tempDir + "/" + fileName;

            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

                qDebug() << "MediaSchemeHandler: Saved video stream (" << data.size() << "bytes) to" << filePath;
                emit mediaSaved(filePath);
                emit mediaPlayRequested(filePath);

                if (!playMediaFile(filePath)) {
                    qWarning() << "MediaSchemeHandler: Failed to launch media player for:" << filePath;
                }
            } else {
                qWarning() << "MediaSchemeHandler: Failed to write media file to:" << filePath;
            }
        } else {
            qWarning() << "MediaSchemeHandler: Received empty request payload for wa-media://play";
        }

        QBuffer *replyBuffer = new QBuffer(job);
        replyBuffer->open(QIODevice::ReadWrite);
        replyBuffer->write("OK");
        replyBuffer->seek(0);
        job->reply("text/plain", replyBuffer);
        return;
    }

    if (url.host() == "caller" || url.path().contains("caller")) {
        QByteArray data;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QIODevice *payload = job->requestBody();
        if (payload) {
            if (!payload->isOpen()) {
                payload->open(QIODevice::ReadOnly);
            }
            data = payload->readAll();
        }
#endif

        QString callerName;
        bool isVideo = false;
        QString avatarPath;

        if (!data.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                callerName = obj.value("name").toString().trimmed();
                isVideo = obj.value("isVideo").toBool();
                QString avatarData = obj.value("avatar").toString();

                if (!avatarData.isEmpty() && avatarData.contains("base64,")) {
                    int commaIdx = avatarData.indexOf("base64,");
                    if (commaIdx != -1) {
                        QByteArray base64Data = avatarData.mid(commaIdx + 7).toLatin1();
                        QByteArray imgBytes = QByteArray::fromBase64(base64Data);
                        if (!imgBytes.isEmpty()) {
                            QString avatarDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/avatars";
                            QDir().mkpath(avatarDir);
                            avatarPath = avatarDir + QString("/caller_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
                            QFile file(avatarPath);
                            if (file.open(QIODevice::WriteOnly)) {
                                file.write(imgBytes);
                                file.close();
                                qDebug() << "MediaSchemeHandler: Saved caller avatar to:" << avatarPath << "Size:" << imgBytes.size() << "bytes";
                            }
                        }
                    }
                }
            }
        }

        if (callerName.isEmpty()) {
            callerName = tr("WhatsApp Contact");
        }

        qDebug() << "MediaSchemeHandler: Incoming call notification payload received:" << callerName << "Video:" << isVideo << "Avatar:" << avatarPath;
        emit incomingCallDetected(callerName, isVideo, avatarPath);

        QBuffer *replyBuffer = new QBuffer(job);
        replyBuffer->open(QIODevice::ReadWrite);
        replyBuffer->write("OK");
        replyBuffer->seek(0);
        job->reply("text/plain", replyBuffer);
        return;
    }

    job->fail(QWebEngineUrlRequestJob::UrlNotFound);
}
