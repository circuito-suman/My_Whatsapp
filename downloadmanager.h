#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QWebEngineDownloadRequest>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QPointer>
#include <QWidget>
#include <compare>
#include <concepts>
#include <span>
#include <string_view>

class SystemIntegration;

struct DownloadItem {
    QString id;
    QString fileName;
    QString fullPath;
    qint64 totalBytes = 0;
    qint64 receivedBytes = 0;
    int progress = 0;
    QDateTime timestamp;
    bool isCompleted = false;
    bool isFailed = false;
    QString mimeType;

    // C++20 Spaceship Three-Way Comparison Operator
    std::weak_ordering operator<=>(const DownloadItem &other) const {
        if (auto cmp = timestamp <=> other.timestamp; cmp != 0) return cmp;
        return progress <=> other.progress;
    }
    bool operator==(const DownloadItem &other) const {
        return id == other.id;
    }
};

// C++20 Concept constraining byte size calculations
template <typename T>
concept IntegralByteCount = std::integral<T>;

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(SystemIntegration *systemIntegration, QObject *parent = nullptr);
    ~DownloadManager() override;

    QString downloadDirectory() const;
    void setDownloadDirectory(const QString &dir);

    bool autoDownloadEnabled() const;
    void setAutoDownloadEnabled(bool enabled);

    void handleDownload(QWebEngineDownloadRequest *download, QWidget *parentWidget = nullptr);
    void showDownloadsDialog(QWidget *parent = nullptr);

    const QList<DownloadItem>& downloadHistory() const { return m_history; }
    void sortHistoryByProgress();

    [[nodiscard("Sanitization Result: Do not discard sanitized file name")]]
    static QString sanitizeFileName(const QString &fileName, const QString &mimeType);

    [[nodiscard("Unique File Path: Do not discard generated unique file path")]]
    static QString getUniqueFilePath(const QString &dir, const QString &baseName);

    static void applySecureFilePermissions(const QString &filePath);

signals:
    void downloadStarted(const QString &fileName);
    void downloadProgress(const QString &fileName, int percent);
    void downloadCompleted(const QString &fileName, const QString &fullPath);
    void downloadFailed(const QString &fileName, const QString &reason);

private:
    QString m_downloadDirectory;
    bool m_autoDownload;
    QList<DownloadItem> m_history;
    SystemIntegration *m_systemIntegration;
};

#endif // DOWNLOADMANAGER_H
