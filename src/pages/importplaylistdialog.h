#ifndef IMPORTPLAYLISTDIALOG_H
#define IMPORTPLAYLISTDIALOG_H

#include <QDialog>
#include <QList>
#include "trackmodel.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QNetworkAccessManager;
class QProcess;

// Converts a playlist from a streaming service into a Lumen Music playlist.
//
//  - Spotify: reads the public embed page (no API key needed) to get the
//    playlist name and its tracks, then matches each one against a YouTube
//    search. The user reviews the matches (checkbox = approve) before any
//    download starts, so wrong matches can be rejected.
//  - YouTube / YouTube Music: lists the playlist with yt-dlp --flat-playlist;
//    entries are exact, so they only go through the same approval list.
//
// Files are downloaded into a subfolder of the downloads root named after the
// playlist, and the final Lumen playlist keeps the original track order.
class ImportPlaylistDialog : public QDialog {
    Q_OBJECT
public:
    ImportPlaylistDialog(TrackModel *model, const QString &url, QWidget *parent = nullptr);

    // Whether the given URL looks like a playlist this dialog can import.
    static bool canImport(const QString &url);

protected:
    void reject() override;

private:
    struct ImportItem {
        QString title;
        QString artist;
        qint64  durationMs = 0;   // from the streaming service, 0 = unknown

        // Best YouTube match (filled by the matching phase).
        QString matchTitle;
        QString matchUrl;
        qint64  matchDurationS = 0;
        bool    confident = false;
    };

    void fetchMetadata();
    void fetchSpotify(const QString &type, const QString &id);
    void fetchYouTubePlaylist();
    void parseSpotifyEmbed(const QByteArray &html);
    void onMetadataReady();
    void showError(const QString &message);

    // Matching phase (Spotify only): one yt-dlp search per track.
    void findNextMatch();
    void onMatchingDone();
    void updateItemRow(int index);

    void startDownloads();
    void downloadNext();
    void finishImport();

    TrackModel *m_model;
    QString m_url;
    QString m_playlistName;
    QList<ImportItem> m_items;
    QList<int> m_downloadQueue;   // indexes of approved items, in playlist order
    QList<int> m_importedIds;     // library ids in playlist order, for reordering

    QLabel      *m_statusLabel;
    QLineEdit   *m_nameEdit;
    QListWidget *m_listWidget;
    QPushButton *m_startBtn;
    QPushButton *m_cancelBtn;

    QNetworkAccessManager *m_net = nullptr;
    QProcess *m_proc = nullptr;
    QString   m_downloadPrefix;
    int  m_matchIndex = -1;
    int  m_queuePos   = -1;
    int  m_okCount    = 0;
    bool m_needsMatching = false;
    bool m_running    = false;
    bool m_cancelled  = false;
    bool m_ytdlpRetried = false;
};

#endif // IMPORTPLAYLISTDIALOG_H
