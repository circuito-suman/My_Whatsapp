#ifndef MEDIASCHEMEHANDLER_H
#define MEDIASCHEMEHANDLER_H

#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#include <QObject>
#include <span>
#include <string_view>
#include <concepts>

// C++20 Transcode options with designated initializer support
struct MediaTranscodeOptions {
    int videoBitrateKbps = 1500;
    int cpuUsed = 8;
    bool hardwareAcceleration = true;
};

// C++20 Concept to validate contiguous byte payloads
template <typename T>
concept ByteContiguous = requires(T b) {
    { b.data() } -> std::convertible_to<const char*>;
    { b.size() } -> std::convertible_to<std::size_t>;
};

class MediaSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit MediaSchemeHandler(QObject *parent = nullptr);
    ~MediaSchemeHandler() override = default;

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    static void registerScheme();

    [[nodiscard("Playback Execution: Caller must verify return value of playMediaFile()")]]
    static bool playMediaFile(const QString &filePath);

    [[nodiscard("MIME Type: Caller must not discard detected MIME type")]]
    static std::string_view detectMimeType(std::span<const uint8_t> bytes);

signals:
    void mediaSaved(const QString &filePath);
    void mediaPlayRequested(const QString &filePath);
    void incomingCallDetected(const QString &callerName, bool isVideo, const QString &avatarPath);
};

#endif // MEDIASCHEMEHANDLER_H
