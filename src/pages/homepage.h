#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "trackmodel.h"

class TrackContextMenu;

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

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *createTrackRow(const Track &track, int index, int currentId, bool isPlaying);
    QWidget *createFolderChip(const Folder &folder, int trackCount);
    QWidget *createChipCover(const Folder &folder);
    QWidget *createRecentCard(const Folder &folder);
    int chipColumnsForWidth(int w) const;

    TrackModel *m_model;
    TrackContextMenu *m_ctx = nullptr;
    QVBoxLayout *m_contentLayout;
    QScrollArea *m_scroll;
    int m_lastChipCols = -1;
    int m_lastCurrentId = 0;
    bool m_lastPlaying = false;
};

#endif // HOMEPAGE_H