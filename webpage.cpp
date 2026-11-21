#include "webpage.h"
#include "screensharedialog.h"
#include "popoutwindow.h"
#include <QWebEngineSettings>
#include <QDesktopServices>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#include <QWidget>

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    // Screen sharing enabled
    settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);

    // Modern Qt 6.7+ permission handler
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(this, &QWebEnginePage::permissionRequested, this, &WebPage::handlePermissionRequested);
#endif

    // Connect featurePermissionRequested for full backward and forward compatibility
    connect(this, &QWebEnginePage::featurePermissionRequested, this, [this](const QUrl &securityOrigin, QWebEnginePage::Feature feature) {
        const QString host = securityOrigin.host();
        if (isTrustedHost(host) || host.isEmpty() || securityOrigin.toString().contains("whatsapp")) {
            qDebug() << "WebPage: Granting feature permission" << static_cast<int>(feature) << "to" << securityOrigin;
            setFeaturePermission(securityOrigin, feature, QWebEnginePage::PermissionGrantedByUser);
        } else {
            qWarning() << "WebPage: Denying feature permission" << static_cast<int>(feature) << "to untrusted origin:" << securityOrigin;
            setFeaturePermission(securityOrigin, feature, QWebEnginePage::PermissionDeniedByUser);
        }
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    // Desktop Media Capture (Screen Sharing) for WhatsApp Calls
    connect(this, &QWebEnginePage::desktopMediaRequested, this, &WebPage::handleDesktopMediaRequested);
#endif

    // SSL Certificate Error logging
    connect(this, &QWebEnginePage::certificateError, this, [](const QWebEngineCertificateError &error) {
        qWarning() << "WebPage: SSL Certificate error:" << error.description() << "for URL:" << error.url();
    });

    // Inject scripts upon page load
    connect(this, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
            injectClientScripts();
        }
    });
}

WebPage::~WebPage() = default;

bool WebPage::isTrustedHost(std::string_view sv)
{
    if (sv.empty()) return false;
    return sv == "web.whatsapp.com" ||
           sv == "whatsapp.com" ||
           sv == "whatsapp.net" ||
           sv == "static.whatsapp.net" ||
           sv == "mmg.whatsapp.net" ||
           sv == "pps.whatsapp.net" ||
           sv.ends_with(".whatsapp.com") ||
           sv.ends_with(".whatsapp.net");
}

bool WebPage::isTrustedHost(const QString &host)
{
    if (host.isEmpty()) return false;
    QByteArray utf8 = host.toUtf8();
    return isTrustedHost(std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())));
}

bool WebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    // Block attempt to navigate to local file or qrc resources (security leak protection)
    if (url.scheme() == "file" || url.scheme() == "qrc") {
        qWarning() << "WebPage: Blocked attempt to navigate to local file/qrc URL (security):" << url;
        return false;
    }

    // Allow internal custom schemes
    if (url.scheme() == "wa-media" || url.scheme() == "about") {
        return true;
    }

    const QString host = url.host();

    if (type == NavigationTypeLinkClicked) {
        if (!isTrustedHost(host)) {
            if (!qEnvironmentVariableIsSet("WHATSAPP_TEST_MODE")) {
                qDebug() << "WebPage: Opening external link in default browser (leak prevention):" << url;
                QDesktopServices::openUrl(url);
            } else {
                qDebug() << "WebPage: Test mode active - suppressed external browser launch for:" << url;
            }
            return false;
        }
    }

    if (isMainFrame && !url.isEmpty()) {
        if (!isTrustedHost(host)) {
            if (!qEnvironmentVariableIsSet("WHATSAPP_TEST_MODE")) {
                qDebug() << "WebPage: Blocked navigation away from WhatsApp (leak prevention):" << url;
                QDesktopServices::openUrl(url);
            } else {
                qDebug() << "WebPage: Test mode active - suppressed external browser launch for:" << url;
            }
            return false;
        }
    }

    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage *WebPage::createWindow(WebWindowType type)
{
    Q_UNUSED(type)
    qDebug() << "WebPage: Spawning native popout window for WhatsApp popup/call request";
    const PopoutWindow *popout = new PopoutWindow(profile());
    return popout->page();
}

void WebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                      const QString &message,
                                      int lineNumber,
                                      const QString &sourceID)
{
    QString levelStr;
    switch (level) {
    case InfoMessageLevel: levelStr = "INFO"; break;
    case WarningMessageLevel: levelStr = "WARN"; break;
    case ErrorMessageLevel: levelStr = "ERROR"; break;
    }
    QString src = QFileInfo(sourceID).fileName();
    if (src.isEmpty()) src = "inline";
    qDebug().noquote() << QString("[JS %1] [%2:%3] %4").arg(levelStr, src, QString::number(lineNumber), message);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void WebPage::handlePermissionRequested(QWebEnginePermission permission)
{
    const QString host = permission.origin().host();
    if (isTrustedHost(host)) {
        qDebug() << "WebPage: Granting permission" << static_cast<int>(permission.permissionType()) << "to" << permission.origin();
        permission.grant();
    } else {
        qWarning() << "WebPage: Denying permission" << static_cast<int>(permission.permissionType()) << "to untrusted host (leak prevention):" << host;
        permission.deny();
    }
}

void WebPage::handleDesktopMediaRequested(const QWebEngineDesktopMediaRequest &request)
{
    qDebug() << "WebPage: Screen sharing requested by WhatsApp call";
    QWidget *parentWidget = qobject_cast<QWidget*>(parent());
    ScreenShareDialog *dialog = new ScreenShareDialog(request, parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
#endif

void WebPage::injectClientScripts()
{
    const QString clientScript = R"(
        (function() {
            if (window.__whatsapp_desktop_injected) return;
            window.__whatsapp_desktop_injected = true;

            console.log("WhatsApp Desktop: Injecting native optimizations...");

            // Ensure navigator.storage.persist returns true for reliable persistence
            try {
                if (navigator.storage && navigator.storage.persist) {
                    navigator.storage.persist = function() {
                        return Promise.resolve(true);
                    };
                }
            } catch(e) {}

            // 1. Stable Viewport CSS (Prevents responsive screen jumping/resizing during loading)
            const style = document.createElement('style');
            style.id = 'whatsapp-desktop-styles';
            style.textContent = `
                html, body, #app {
                    width: 100% !important;
                    height: 100% !important;
                    margin: 0 !important;
                    padding: 0 !important;
                }
                * { cursor: default; }
                input, textarea, [contenteditable="true"] { cursor: text !important; }
                button, a, [role="button"], [data-tab], [data-testid*="button"] { cursor: pointer !important; }
                .wa-native-mpv-btn:hover { background-color: #20BA5C !important; }
            `;
            document.head.appendChild(style);

            // 1b. WebGL NPOT and video frame safety shims (Prevents camera freeze in video calls)
            try {
                function patchGL(gl) {
                    if (!gl || gl.__patched_wa) return;
                    gl.__patched_wa = true;

                    const origTexParameteri = gl.texParameteri;
                    gl.texParameteri = function(target, pname, param) {
                        if (target === gl.TEXTURE_2D) {
                            if (pname === gl.TEXTURE_WRAP_S || pname === gl.TEXTURE_WRAP_T) {
                                param = gl.CLAMP_TO_EDGE;
                            }
                            if (pname === gl.TEXTURE_MIN_FILTER) {
                                if (param === gl.NEAREST_MIPMAP_NEAREST ||
                                    param === gl.LINEAR_MIPMAP_NEAREST ||
                                    param === gl.NEAREST_MIPMAP_LINEAR ||
                                    param === gl.LINEAR_MIPMAP_LINEAR) {
                                    param = gl.LINEAR;
                                }
                            }
                        }
                        return origTexParameteri.call(this, target, pname, param);
                    };

                    const origTexImage2D = gl.texImage2D;
                    gl.texImage2D = function(...args) {
                        for (let i = 0; i < args.length; i++) {
                            const item = args[i];
                            if (item instanceof HTMLVideoElement) {
                                if (item.videoWidth === 0 || item.videoHeight === 0 || item.readyState < 2) {
                                    return;
                                }
                                origTexParameteri.call(this, gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                                origTexParameteri.call(this, gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                                origTexParameteri.call(this, gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
                                origTexParameteri.call(this, gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
                            }
                        }
                        try {
                            return origTexImage2D.apply(this, args);
                        } catch(e) {}
                    };

                    const origTexSubImage2D = gl.texSubImage2D;
                    gl.texSubImage2D = function(...args) {
                        for (let i = 0; i < args.length; i++) {
                            const item = args[i];
                            if (item instanceof HTMLVideoElement) {
                                if (item.videoWidth === 0 || item.videoHeight === 0 || item.readyState < 2) {
                                    return;
                                }
                            }
                        }
                        try {
                            return origTexSubImage2D.apply(this, args);
                        } catch(e) {}
                    };
                }

                const origGetContext = HTMLCanvasElement.prototype.getContext;
                HTMLCanvasElement.prototype.getContext = function(type, ...args) {
                    const ctx = origGetContext.call(this, type, ...args);
                    if (ctx && (type === 'webgl' || type === 'experimental-webgl' || type === 'webgl2')) {
                        patchGL(ctx);
                    }
                    return ctx;
                };
            } catch(e) {}

            // 2. WebRTC Call & Stream Telemetry + Fast Popout Window Teardown
            if (window.RTCPeerConnection) {
                const OrigPC = window.RTCPeerConnection;
                window.RTCPeerConnection = function(...args) {
                    console.log("[WebRTC] New RTCPeerConnection initialized with config:", JSON.stringify(args[0] || {}));
                    const pc = new OrigPC(...args);
                    pc.addEventListener('iceconnectionstatechange', () => {
                        console.log("[WebRTC] ICE Connection State Changed ->", pc.iceConnectionState);
                        if ((pc.iceConnectionState === 'closed' || pc.iceConnectionState === 'disconnected') && (window.location.href.includes('/call/popout') || window.opener)) {
                            try {
                                document.body.style.visibility = 'hidden';
                                window.close();
                            } catch(e) {}
                        }
                    });
                    pc.addEventListener('connectionstatechange', () => {
                        console.log("[WebRTC] Peer Connection State Changed ->", pc.connectionState);
                        if (pc.connectionState === 'closed' && (window.location.href.includes('/call/popout') || window.opener)) {
                            try {
                                document.body.style.visibility = 'hidden';
                                window.close();
                            } catch(e) {}
                        }
                    });
                    pc.addEventListener('signalingstatechange', () => {
                        console.log("[WebRTC] Signaling State Changed ->", pc.signalingState);
                        if (pc.signalingState === 'closed' && (window.location.href.includes('/call/popout') || window.opener)) {
                            try {
                                document.body.style.visibility = 'hidden';
                                window.close();
                            } catch(e) {}
                        }
                    });
                    pc.addEventListener('track', (e) => {
                        console.log("[WebRTC] Remote Media Track Received:", e.track.kind, "id:", e.track.id, "label:", e.track.label);
                    });
                    return pc;
                };
                window.RTCPeerConnection.prototype = OrigPC.prototype;
                console.log("WhatsApp Desktop: WebRTC telemetry instrumentation active.");
            }

            // Fast Popout Hangup Listener: Immediately hide and close popout call window when call ends
            if (window.location.href.includes('/call/popout') || window.opener) {
                document.addEventListener('click', function(e) {
                    const btn = e.target.closest('button[data-testid*="hangup"], button[data-testid*="call-end"], button[aria-label*="End call"], button[aria-label*="Hang up"], button[title*="End call"], button[title*="Hang up"]');
                    if (btn) {
                        setTimeout(() => {
                            try {
                                document.body.style.visibility = 'hidden';
                                window.close();
                            } catch(err) {}
                        }, 50);
                    }
                }, true);

                window.addEventListener('pagehide', function() {
                    try {
                        document.body.style.visibility = 'hidden';
                        window.close();
                    } catch(err) {}
                });
            }

            // 3. Audio/Video media events for MPRIS controls
            document.addEventListener('play', function(e) {
                if (e.target && (e.target.tagName === 'AUDIO' || e.target.tagName === 'VIDEO')) {
                    console.log("[Media] Playback started:", e.target.tagName, e.target.currentSrc || e.target.src);
                }
            }, true);

            document.addEventListener('pause', function(e) {
                if (e.target && (e.target.tagName === 'AUDIO' || e.target.tagName === 'VIDEO')) {
                    console.log("[Media] Playback paused:", e.target.tagName);
                }
            }, true);

            // 4. In-Place Status & Media Video Transcoder & Player
            (function setupVideoInterceptor() {
                const transcodeQueue = new Set();

                async function transcodeVideo(video, src) {
                    if (!src || transcodeQueue.has(src) || video.__wa_transcoded || video.__wa_transcoding) return;
                    transcodeQueue.add(src);
                    video.__wa_transcoding = true;
                    console.log("[StatusVideo] Intercepted video source for in-place transcode:", src);

                    try {
                        const response = await fetch(src);
                        if (!response.ok) throw new Error("Failed to fetch stream: " + response.status);
                        const mp4Blob = await response.blob();

                        console.log("[StatusVideo] Fetched MP4 blob (" + mp4Blob.size + " bytes), sending to wa-media://app/transcode");
                        const webmBlob = await new Promise((resolve, reject) => {
                            const xhr = new XMLHttpRequest();
                            xhr.open('POST', 'wa-media://app/transcode');
                            xhr.responseType = 'blob';
                            xhr.onload = () => {
                                if (xhr.status === 200 || (xhr.response && xhr.response.size > 0)) {
                                    resolve(xhr.response);
                                } else {
                                    reject(new Error("Transcode XHR status: " + xhr.status));
                                }
                            };
                            xhr.onerror = () => reject(new Error("Transcode XHR network error"));
                            xhr.send(mp4Blob);
                        });

                        console.log("[StatusVideo] Transcode complete (" + webmBlob.size + " bytes WebM). Playing in-place.");

                        const webmUrl = URL.createObjectURL(webmBlob);
                        video.__wa_transcoding = false;
                        video.__wa_transcoded = true;
                        video.src = webmUrl;
                        video.load();
                        video.play().catch(e => console.log("[StatusVideo] Play error:", e));
                    } catch (e) {
                        video.__wa_transcoding = false;
                        console.warn("[StatusVideo] In-place transcode error:", e);
                    } finally {
                        transcodeQueue.delete(src);
                    }
                }

                function scanVideos() {
                    const videos = document.querySelectorAll('video');
                    for (let i = 0; i < videos.length; i++) {
                        const v = videos[i];
                        if (v.__wa_transcoded || v.__wa_transcoding) continue;
                        const src = v.currentSrc || v.src;
                        if (src && v.error) {
                            transcodeVideo(v, src);
                        }
                    }
                }

                document.addEventListener('error', function(e) {
                    if (e.target && e.target.tagName === 'VIDEO') {
                        const v = e.target;
                        const src = v.currentSrc || v.src;
                        if (src && !v.__wa_transcoded) {
                            transcodeVideo(v, src);
                        }
                    }
                }, true);

                const videoObserver = new MutationObserver(() => {
                    scanVideos();
                });
                videoObserver.observe(document.body, { childList: true, subtree: true });
            })();
        })();
    )";

    runJavaScript(clientScript);
}

void WebPage::applyTheme(bool dark)
{
    const QString themeScript = QString(R"(
        (function() {
            try {
                if (window.localStorage) {
                    const targetTheme = '%1';
                    localStorage.setItem('theme', targetTheme);
                    if (document.body) {
                        document.body.classList.toggle('dark', %2);
                    }
                    window.dispatchEvent(new Event('storage'));
                }
            } catch(e) {
                // Ignore storage access before origin is bound
            }
        })();
    )").arg(dark ? "dark" : "light").arg(dark ? "true" : "false");

    runJavaScript(themeScript);
}

void WebPage::triggerNewChat()
{
    runJavaScript(R"(
        (function() {
            const newChatBtn = document.querySelector('[data-testid="chat"], [aria-label="New chat"], [data-icon="chat"]');
            if (newChatBtn) {
                newChatBtn.click();
            } else {
                document.dispatchEvent(new KeyboardEvent('keydown', { key: 'n', code: 'KeyN', ctrlKey: true, altKey: true, bubbles: true }));
            }
        })();
    )");
}

void WebPage::triggerSearch()
{
    runJavaScript(R"(
        (function() {
            const searchInput = document.querySelector('[data-testid="chat-list-search"], div[contenteditable="true"][data-tab="3"]');
            if (searchInput) {
                searchInput.focus();
            } else {
                document.dispatchEvent(new KeyboardEvent('keydown', { key: '/', code: 'Slash', ctrlKey: true, bubbles: true }));
            }
        })();
    )");
}

void WebPage::triggerPreviousChat()
{
    runJavaScript(R"(
        document.dispatchEvent(new KeyboardEvent('keydown', { key: '[', code: 'BracketLeft', ctrlKey: true, shiftKey: true, bubbles: true }));
    )");
}

void WebPage::triggerNextChat()
{
    runJavaScript(R"(
        document.dispatchEvent(new KeyboardEvent('keydown', { key: ']', code: 'BracketRight', ctrlKey: true, shiftKey: true, bubbles: true }));
    )");
}

void WebPage::triggerToggleMediaPlayback()
{
    runJavaScript(R"(
        (function() {
            const mediaList = document.querySelectorAll('audio, video');
            for (let i = 0; i < mediaList.length; i++) {
                const m = mediaList[i];
                if (!m.paused) {
                    m.pause();
                    return;
                }
            }
            if (mediaList.length > 0) {
                mediaList[mediaList.length - 1].play();
            }
        })();
    )");
}
