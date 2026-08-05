#ifndef FOLDERSPAGE_H
#define FOLDERSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "trackmodel.h"
#include "theme.h"

class FoldersPage : public QWidget {
    Q_OBJECT
public:
    explicit FoldersPage(TrackModel *model, QWidget *parent = nullptr);
    void refresh();

signals:
    void folderSelected(const QString &folderName);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void showCreateDialog();
    void showRenameDialog(int id, const QString &currentName);
    void showCoverDialog(int id, const Theme::GradientPair &current, const QString &currentImage);
    void showDeleteConfirm(int id, const QString &name);

private:
    int columnCountForWidth(int w) const;

    TrackModel *m_model;
    QVBoxLayout *m_contentLayout;
    int m_lastCols = -1;
};

#endif // FOLDERSPAGE_H
