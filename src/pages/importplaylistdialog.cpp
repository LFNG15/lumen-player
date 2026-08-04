#include "importplaylistdialog.h"
#include "database.h"
#include "tools/ytdlp_bootstrap.h"
#include "lang.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include "theme.h"
#include "design/stylesheet.h"
#include "mediatools.h"

static const QRegularExpression kSpotifyRe(
    R"(open\.spotify\.com/(?:intl-[a-z\-]+/)?(playlist|album)/([A-Za-z0-9]+))");

bool ImportPlaylistDialog::canImport(const QString &url) {
    if (kSpotifyRe.match(url).hasMatch()) return true;
    return (url.contains("youtube.com") || url.contains("youtu.be"))
            && url.contains("list=");
}

ImportPlaylistDialog::ImportPlaylistDialog(TrackModel *model, const QString &url, QWidget *parent)
    : QDialog(parent), m_model(model), m_url(url.trimmed())
{
    setWindowTitle(Lang::tr("Importar Playlist"));
    setFixedSize(560, 600);
    setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(this, QString(
        "QDialog { background: %1; }"
        "QLabel { background: transparent; color: %2; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 8px 12px; }"
        "QLineEdit:focus { border-color: %5; }"
        "QListWidget { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 4px; }"
        "QListWidget::item { padding: 5px 8px; border-radius: 4px; }"
        "QListWidget::indicator { width: 14px; height: 14px; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::bg().name(),
          Theme::border().name(), Theme::accent().name()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *title = new QLabel(Lang::tr("Importar playlist de streaming"));
    title->setFont(Theme::titleFont(16));
    layout->addWidget(title);

    m_statusLabel = new QLabel(Lang::tr("Buscando informações da playlist..."));
    m_statusLabel->setFont(Theme::bodyFont(11));
    m_statusLabel->setWordWrap(true);
    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    layout->addWidget(m_statusLabel);

    auto *nameLabel = new QLabel(Lang::tr("Nome da playlist no Lumen Music"));
    nameLabel->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    layout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setFont(Theme::bodyFont(13));
    m_nameEdit->setEnabled(false);
    layout->addWidget(m_nameEdit);

    m_listWidget = new QListWidget();
    m_listWidget->setFont(Theme::bodyFont(10));
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_listWidget->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_listWidget, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    m_cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    m_cancelBtn->setFont(Theme::bodyFont(12));
    m_cancelBtn->setFixedHeight(36);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(m_cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; padding: 0 16px; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.05) + "; }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_cancelBtn);

    m_startBtn = new QPushButton(Lang::tr("Importar"));
    m_startBtn->setFont(Theme::bodyFont(12));
    m_startBtn->setFixedHeight(36);
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setEnabled(false);
    lumen::design::StyleSheet::apply(m_startBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 18px; padding: 0 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }"
    ).arg(Theme::accent().name(), Theme::onAccent().name(), Theme::accentHover().name(),
          Theme::border().name(), Theme::textMuted().name()));
    connect(m_startBtn, &QPushButton::clicked, this, &ImportPlaylistDialog::startDownloads);
    btnRow->addWidget(m_startBtn);

    layout->addLayout(btnRow);

    QTimer::singleShot(0, this, &ImportPlaylistDialog::fetchMetadata);
}

void ImportPlaylistDialog::reject() {
    m_cancelled = true;
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
    QDialog::reject();
}

// ── Metadata ────────────────────────────────────────────────

void ImportPlaylistDialog::fetchMetadata() {
    auto spotify = kSpotifyRe.match(m_url);
    if (spotify.hasMatch()) {
        m_needsMatching = true;
        fetchSpotify(spotify.captured(1), spotify.captured(2));
    } else {
        fetchYouTubePlaylist();
    }
}

void ImportPlaylistDialog::fetchSpotify(const QString &type, const QString &id) {
    if (!m_net) m_net = new QNetworkAccessManager(this);

    // The public embed page carries the playlist metadata as JSON — no login
    // or API key needed.
    QNetworkRequest req(QUrl(QString("https://open.spotify.com/embed/%1/%2").arg(type, id)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0 Safari/537.36");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            showError(Lang::tr("Não foi possível acessar o Spotify: ") + reply->errorString());
            return;
        }
        parseSpotifyEmbed(reply->readAll());
    });
}

// Depth-first search for the JSON object that holds the playlist data — the
// page structure changes between Spotify releases, but the entity always has
// a "trackList" array next to its display name.
static QJsonObject findTrackListObject(const QJsonValue &v) {
    if (v.isObject()) {
        QJsonObject o = v.toObject();
        if (o.value("trackList").isArray()) return o;
        for (auto it = o.begin(); it != o.end(); ++it) {
            QJsonObject r = findTrackListObject(it.value());
            if (!r.isEmpty()) return r;
        }
    } else if (v.isArray()) {
        for (const auto &x : v.toArray()) {
            QJsonObject r = findTrackListObject(x);
            if (!r.isEmpty()) return r;
        }
    }
    return {};
}

// The embed's duration field has shifted between ms and seconds over time, so
// treat large values as ms.
static qint64 durationToMs(const QJsonValue &v) {
    if (v.isDouble()) {
        qint64 n = static_cast<qint64>(v.toDouble());
        return n > 30000 ? n : n * 1000;
    }
    return 0;
}

void ImportPlaylistDialog::parseSpotifyEmbed(const QByteArray &html) {
    // Collect every inline JSON <script> blob and look for the track list in
    // each ( __NEXT_DATA__ today, but stay tolerant to renames).
    static const QRegularExpression scriptRe(
        R"(<script[^>]*type="application/json"[^>]*>(.*?)</script>)",
        QRegularExpression::DotMatchesEverythingOption);

    QString page = QString::fromUtf8(html);
    QJsonObject entity;
    auto it = scriptRe.globalMatch(page);
    while (it.hasNext() && entity.isEmpty()) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(it.next().captured(1).toUtf8(), &err);
        if (err.error == QJsonParseError::NoError)
            entity = findTrackListObject(doc.isArray() ? QJsonValue(doc.array())
                                                       : QJsonValue(doc.object()));
    }

    if (entity.isEmpty()) {
        showError(Lang::tr("Não foi possível ler a playlist do Spotify. Playlists privadas não podem ser importadas — torne a playlist pública e tente novamente."));
        return;
    }

    m_playlistName = entity.value("name").toString();
    if (m_playlistName.isEmpty()) m_playlistName = entity.value("title").toString();
    if (m_playlistName.isEmpty()) m_playlistName = Lang::tr("Playlist importada");

    for (const auto &v : entity.value("trackList").toArray()) {
        QJsonObject t = v.toObject();
        ImportItem item;
        item.title      = t.value("title").toString();
        item.artist     = t.value("subtitle").toString();
        item.durationMs = durationToMs(t.value("duration"));
        if (item.title.isEmpty()) continue;
        m_items.append(item);
    }

    onMetadataReady();
}

void ImportPlaylistDialog::fetchYouTubePlaylist() {
    QString ytErr;
    const QString ytDlp = lumen::tools::ensureYtDlp(&ytErr);
    if (ytDlp.isEmpty()) {
        showError(ytErr.isEmpty()
            ? Lang::tr("yt-dlp não encontrado. Verifique sua pasta de instalação.")
            : ytErr);
        return;
    }

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus st) {
        QByteArray out = m_proc->readAllStandardOutput();
        QByteArray err = m_proc->readAllStandardError();
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_cancelled) return;

        QJsonDocument doc = QJsonDocument::fromJson(out);
        if (exitCode != 0 || st != QProcess::NormalExit || !doc.isObject()) {
            // yt-dlp reports private/members-only playlists on stderr — surface
            // that distinctly instead of the generic "check the link" message.
            if (err.contains("private") || err.contains("Private"))
                showError(Lang::tr("Esta playlist do YouTube é privada e não pode ser importada. Torne-a pública ou não listada e tente novamente."));
            else
                showError(Lang::tr("Não foi possível listar a playlist do YouTube. Verifique o link."));
            return;
        }

        QJsonObject root = doc.object();
        m_playlistName = root.value("title").toString();
        if (m_playlistName.isEmpty()) m_playlistName = Lang::tr("Playlist importada");

        for (const auto &v : root.value("entries").toArray()) {
            QJsonObject e = v.toObject();
            ImportItem item;
            item.title  = e.value("title").toString();
            item.artist = e.value("uploader").toString();
            if (item.title.isEmpty() || e.value("url").toString().isEmpty()) continue;
            if (item.artist.isEmpty()) item.artist = Lang::tr("Desconhecido");
            // Playlist entries are exact, no matching needed.
            item.matchTitle      = item.title;
            item.matchUrl        = e.value("url").toString();
            item.matchDurationS  = static_cast<qint64>(e.value("duration").toDouble());
            item.confident       = true;
            m_items.append(item);
        }
        onMetadataReady();
    });

    m_proc->start(ytDlp, {"-J", "--flat-playlist", m_url});
}

void ImportPlaylistDialog::onMetadataReady() {
    if (m_items.isEmpty()) {
        showError(Lang::tr("Nenhuma música encontrada nessa playlist."));
        return;
    }

    m_nameEdit->setText(m_playlistName);
    m_nameEdit->setEnabled(true);

    m_listWidget->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        auto *li = new QListWidgetItem(m_listWidget);
        li->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        updateItemRow(i);
    }

    if (m_needsMatching) {
        m_matchIndex = -1;
        findNextMatch();
    } else {
        onMatchingDone();
    }
}

void ImportPlaylistDialog::showError(const QString &message) {
    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(Theme::danger().name()));
    m_statusLabel->setText(message);
}

// ── YouTube matching (Spotify imports) ──────────────────────

// Lowercase, strip accents and punctuation so titles compare loosely.
static QString normToken(const QString &s) {
    QString d = s.normalized(QString::NormalizationForm_D).toLower();
    QString out;
    out.reserve(d.size());
    for (const QChar &c : d) {
        if (c.category() == QChar::Mark_NonSpacing) continue;
        out.append(c.isLetterOrNumber() ? c : QChar(' '));
    }
    return out.simplified();
}

// Heuristic match score between a Spotify track and a YouTube search result.
// Title containment and duration proximity weigh the most; the artist often
// appears in either the video title or the channel name.
struct CandidateScore { int score = -1000; bool confident = false; };

static CandidateScore scoreCandidate(const QString &wantTitle, const QString &wantArtist,
                                     qint64 wantMs, const QString &candTitle,
                                     const QString &candChannel, qint64 candS) {
    const QString nt = normToken(wantTitle);
    const QString hay = normToken(candTitle + " " + candChannel);

    CandidateScore r;
    r.score = 0;

    bool titleHit = !nt.isEmpty() && hay.contains(nt);
    if (titleHit) {
        r.score += 3;
    } else {
        // Partial credit: most of the title's words appear somewhere.
        const QStringList words = nt.split(' ', Qt::SkipEmptyParts);
        int hits = 0;
        for (const auto &w : words)
            if (w.size() > 1 && hay.contains(w)) ++hits;
        if (!words.isEmpty() && hits * 2 >= words.size()) r.score += 1;
    }

    bool artistHit = false;
    const QStringList artists = wantArtist.split(QRegularExpression("[,&]"), Qt::SkipEmptyParts);
    for (const auto &a : artists) {
        QString na = normToken(a);
        if (!na.isEmpty() && hay.contains(na)) { artistHit = true; break; }
    }
    if (artistHit) r.score += 2;

    bool durationHit = false;
    if (wantMs > 0 && candS > 0) {
        qint64 diff = qAbs(candS - wantMs / 1000);
        if (diff <= 10)      { r.score += 3; durationHit = true; }
        else if (diff <= 25) { r.score += 1; durationHit = true; }
        else if (diff > 90)  { r.score -= 3; }
    }

    // Confident = the title matched outright plus at least one corroboration,
    // or everything else lines up around a partial title.
    r.confident = (titleHit && (artistHit || durationHit))
               || (r.score >= 5);
    return r;
}

void ImportPlaylistDialog::findNextMatch() {
    if (m_cancelled) return;

    ++m_matchIndex;
    if (m_matchIndex >= m_items.size()) {
        onMatchingDone();
        return;
    }

    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    m_statusLabel->setText(QString(Lang::tr("Buscando correspondências no YouTube... (%1 de %2)"))
        .arg(m_matchIndex + 1).arg(m_items.size()));
    if (auto *li = m_listWidget->item(m_matchIndex)) m_listWidget->scrollToItem(li);

    const ImportItem &item = m_items[m_matchIndex];
    const QString query = QString("ytsearch5:%1 - %2")
        .arg(item.artist.isEmpty() ? QStringLiteral("música") : item.artist, item.title);

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus st) {
        QByteArray out = m_proc->readAllStandardOutput();
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_cancelled) return;

        ImportItem &item = m_items[m_matchIndex];
        if (exitCode == 0 && st == QProcess::NormalExit) {
            QJsonDocument doc = QJsonDocument::fromJson(out);
            int best = -1000;
            for (const auto &v : doc.object().value("entries").toArray()) {
                QJsonObject e = v.toObject();
                const QString candTitle   = e.value("title").toString();
                const QString candChannel = e.value("channel").toString().isEmpty()
                    ? e.value("uploader").toString() : e.value("channel").toString();
                const qint64 candS = static_cast<qint64>(e.value("duration").toDouble());
                const QString candUrl = e.value("url").toString();
                if (candTitle.isEmpty() || candUrl.isEmpty()) continue;

                auto sc = scoreCandidate(item.title, item.artist, item.durationMs,
                                         candTitle, candChannel, candS);
                if (sc.score > best) {
                    best = sc.score;
                    item.matchTitle     = candTitle;
                    item.matchUrl       = candUrl;
                    item.matchDurationS = candS;
                    item.confident      = sc.confident;
                }
            }
        }
        updateItemRow(m_matchIndex);
        findNextMatch();
    });

    m_proc->start(lumen::tools::ensureYtDlp(), {"-J", "--flat-playlist", query});
}

void ImportPlaylistDialog::onMatchingDone() {
    if (m_cancelled) return;

    int needReview = 0;
    for (const auto &item : m_items)
        if (!item.matchUrl.isEmpty() && !item.confident) ++needReview;

    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(
        needReview > 0 ? Theme::danger().name() : Theme::textSoft().name()));
    m_statusLabel->setText(needReview > 0
        ? QString(Lang::tr("Revise a lista: %1 música%2 em laranja podem estar erradas. Marque para aprovar e desmarque para reprovar."))
            .arg(needReview).arg(needReview != 1 ? "s" : "")
        : Lang::tr("Tudo pronto. Desmarque alguma música para não importá-la."));
    m_startBtn->setEnabled(true);
}

void ImportPlaylistDialog::updateItemRow(int index) {
    auto *li = m_listWidget->item(index);
    if (!li) return;
    const ImportItem &item = m_items[index];

    QString text = QString("%1. %2 — %3")
        .arg(index + 1, 2, 10, QChar('0')).arg(item.artist, item.title);

    if (!item.matchUrl.isEmpty()) {
        QString dur = item.matchDurationS > 0
            ? QString(" [%1]").arg(Theme::formatTime(item.matchDurationS * 1000)) : QString();
        text += QString("\n      → %1%2").arg(item.matchTitle, dur);
        li->setCheckState(item.confident ? Qt::Checked : Qt::Unchecked);
        li->setForeground(item.confident ? Theme::text() : QColor(Theme::danger()));
    } else if (m_needsMatching) {
        if (index <= m_matchIndex) {
            text += "\n      ✗ " + Lang::tr("Nenhuma correspondência encontrada");
            li->setCheckState(Qt::Unchecked);
            li->setForeground(Theme::textMuted());
            li->setFlags(li->flags() & ~Qt::ItemIsUserCheckable);
        } else {
            li->setCheckState(Qt::Unchecked);
            li->setForeground(Theme::textMuted());
        }
    } else {
        li->setCheckState(Qt::Checked);
    }
    li->setText(text);
}

// ── Download pipeline ───────────────────────────────────────

void ImportPlaylistDialog::startDownloads() {
    if (m_running) return;

    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) return;
    m_playlistName = name;

    if (lumen::tools::ensureYtDlp().isEmpty()) {
        showError(Lang::tr("yt-dlp não encontrado. Verifique sua pasta de instalação."));
        return;
    }

    // Only approved (checked) tracks are downloaded, in playlist order.
    m_downloadQueue.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        auto *li = m_listWidget->item(i);
        if (li && li->checkState() == Qt::Checked && !m_items[i].matchUrl.isEmpty())
            m_downloadQueue.append(i);
    }
    if (m_downloadQueue.isEmpty()) {
        showError(Lang::tr("Nenhuma música selecionada para importar."));
        return;
    }

    // Freeze the checkboxes while downloading.
    for (int i = 0; i < m_listWidget->count(); ++i)
        m_listWidget->item(i)->setFlags(Qt::ItemIsEnabled);

    // Create the playlist up front so it shows up immediately.
    auto pal = Theme::randomPalette();
    m_model->createPlaylist(m_playlistName, pal.c1, pal.c2);

    m_running = true;
    m_okCount = 0;
    m_queuePos = -1;
    m_importedIds.clear();
    m_nameEdit->setEnabled(false);
    m_startBtn->setEnabled(false);
    downloadNext();
}

void ImportPlaylistDialog::downloadNext() {
    if (m_cancelled) return;

    ++m_queuePos;
    if (m_queuePos >= m_downloadQueue.size()) {
        finishImport();
        return;
    }

    const int index = m_downloadQueue[m_queuePos];
    const ImportItem &item = m_items[index];
    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    m_statusLabel->setText(QString(Lang::tr("Baixando %1 de %2: %3"))
        .arg(m_queuePos + 1).arg(m_downloadQueue.size()).arg(item.title));

    if (auto *li = m_listWidget->item(index)) {
        li->setForeground(Theme::accent());
        m_listWidget->scrollToItem(li);
    }

    // Each playlist gets its own subfolder under the downloads root, so the
    // files are easy to find afterwards.
    const QString outDir = Database::instance().playlistDiskPathByName(m_playlistName);
    m_downloadPrefix = QString("%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(index);
    const QString outTemplate = outDir + "/" + m_downloadPrefix + "_%(title)s.%(ext)s";

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc, &QProcess::finished, this, [this, outDir, index](int exitCode, QProcess::ExitStatus st) {
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_cancelled) return;

        QString file;
        if (exitCode == 0 && st == QProcess::NormalExit) {
            for (const auto &f : QDir(outDir).entryList({m_downloadPrefix + "_*"}, QDir::Files)) {
                if (MediaTools::audioExts().contains(QFileInfo(f).suffix().toLower())) {
                    file = outDir + "/" + f;
                    break;
                }
            }
        }

        auto *li = m_listWidget->item(index);
        if (file.isEmpty()) {
            if (li) {
                li->setForeground(Theme::danger());
                li->setText(QString("✗ %1").arg(li->text()));
            }
        } else {
            const ImportItem &item = m_items[index];
            // Keep the metadata from the streaming service (clean title/artist)
            // instead of the YouTube video title.
            Track t = Track::create(item.title, item.artist.isEmpty() ? Lang::tr("Desconhecido") : item.artist,
                                    m_playlistName, QUrl::fromLocalFile(file));
            m_importedIds.append(m_model->addTrack(t));
            ++m_okCount;
            if (li) {
                li->setForeground(Theme::accent());
                li->setText(QString("✓ %1").arg(li->text()));
            }
        }
        downloadNext();
    });

    m_proc->start(lumen::tools::ensureYtDlp(),
                  MediaTools::downloadArgs(item.matchUrl, outTemplate));
}

void ImportPlaylistDialog::finishImport() {
    m_running = false;

    // Lock the playlist into the original streaming order (downloads already
    // run in order, but this makes it explicit and failure-proof).
    if (!m_importedIds.isEmpty())
        m_model->reorderPlaylist(m_playlistName, m_importedIds);

    lumen::design::StyleSheet::apply(m_statusLabel, QString("color: %1; background: transparent;").arg(
        m_okCount > 0 ? Theme::accent().name() : Theme::danger().name()));
    m_statusLabel->setText(QString(Lang::tr("Concluído! %1 de %2 música%3 importada%3 para \"%4\"."))
        .arg(m_okCount).arg(m_downloadQueue.size())
        .arg(m_downloadQueue.size() != 1 ? "s" : "")
        .arg(m_playlistName));
    m_cancelBtn->setText(Lang::tr("Fechar"));
    m_startBtn->hide();
}
