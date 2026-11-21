#ifndef MEDIAGALLERY_H
#define MEDIAGALLERY_H

#include <QDockWidget>
#include <QPushButton>
#include <QListWidget>

class MediaGallery : public QDockWidget {
    Q_OBJECT
public:
    explicit MediaGallery(QWidget *parent = nullptr);

signals:
    void mediaItemClicked(QListWidgetItem *item);

private slots:
    void onRefreshClicked();
    void onOpenSelectedClicked();

private:
    void refreshGallery();
    QPushButton *refreshButton;
    QPushButton *openButton;
    QListWidget *mediaList;
};

#endif // MEDIAGALLERY_H
