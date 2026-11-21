#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <QtGlobal>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QWebEnginePermission>
#include <QWebEngineDesktopMediaRequest>
#endif
#include <QWebEngineCertificateError>
#include <QUrl>
#include <string_view>
#include <concepts>
#include <span>

template <typename T>
concept HostNameLike = std::convertible_to<T, std::string_view> || std::same_as<T, QString>;

class WebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit WebPage(QWebEngineProfile *profile, QObject *parent = nullptr);
    ~WebPage() override;

    void applyTheme(bool dark);
    void triggerNewChat();
    void triggerSearch();
    void triggerPreviousChat();
    void triggerNextChat();
    void triggerToggleMediaPlayback();

    [[nodiscard("Security Check: Caller must verify return value of isTrustedHost()")]]
    static bool isTrustedHost(std::string_view host);

    [[nodiscard("Security Check: Caller must verify return value of isTrustedHost()")]]
    static bool isTrustedHost(const QString &host);

    [[nodiscard("Security Check: Caller must verify return value of isTrustedHost()")]]
    static bool isTrustedHost(const char *host) {
        return isTrustedHost(std::string_view(host ? host : ""));
    }

signals:
    void incomingCallDetected(const QString &callerName, bool isVideo);
    void unreadCountChanged(int count);
    void mediaPlaybackStateChanged(bool isPlaying);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    QWebEnginePage *createWindow(WebWindowType type) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;

private slots:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void handlePermissionRequested(QWebEnginePermission permission);
    void handleDesktopMediaRequested(const QWebEngineDesktopMediaRequest &request);
#endif

private:
    void injectClientScripts();
};

#endif // WEBPAGE_H
