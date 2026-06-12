#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "trackmodel.h"

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(TrackModel *model, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void deleteRequested(int id);
    void navigateTo(const QString &page, const QString &data = "");

private:
    QWidget *createTrackRow(const Track &track, int index, int currentId, bool isPlaying);
    QWidget *createFolderChip(const Folder &folder, int trackCount);
    QWidget *createChipCover(const Folder &folder);
    QWidget *createRecentCard(const Folder &folder);

    TrackModel *m_model;
    QVBoxLayout *m_contentLayout;
    QScrollArea *m_scroll;
};

#endif // HOMEPAGE_H