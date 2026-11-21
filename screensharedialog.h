#ifndef SCREENSHAREDIALOG_H
#define SCREENSHAREDIALOG_H

#include <QtGlobal>
#include <QDialog>
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QWebEngineDesktopMediaRequest>
#endif
#include <QTabWidget>
#include <QListView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <span>
#include <concepts>

// C++20 Stream Configuration with Designated Initializer support
struct CaptureStreamConfig {
    int targetFps = 30;
    int maxResolutionWidth = 1920;
    int maxResolutionHeight = 1080;
    bool enableAudioShare = true;
};

class ScreenShareDialog : public QDialog
{
    Q_OBJECT
public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    explicit ScreenShareDialog(const QWebEngineDesktopMediaRequest &request, QWidget *parent = nullptr);
#else
    explicit ScreenShareDialog(QWidget *parent = nullptr);
#endif
    ~ScreenShareDialog() override;

protected:
    void reject() override;

private slots:
    void onShareClicked();
    void onCancelClicked();
    void onItemDoubleClicked(const QModelIndex &index);
    void updateSelectionAndState();
    void onTabChanged(int index);

private:
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QWebEngineDesktopMediaRequest m_request;
#endif
    QTabWidget *m_tabWidget;
    QListView *m_screensView;
    QListView *m_windowsView;
    QLabel *m_statusLabel;
    QPushButton *m_shareButton;
    QPushButton *m_cancelButton;
    bool m_handled;
};

#endif // SCREENSHAREDIALOG_H
