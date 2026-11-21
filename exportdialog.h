#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QObject>
#include <QWebEngineView>

class ExportDialog : public QObject {
    Q_OBJECT
public:
    explicit ExportDialog(QWebEngineView *webView, QObject *parent = nullptr);
public slots:
    void exportChatToCSV();
private:
    QWebEngineView *webView;
};

#endif // EXPORTDIALOG_H
