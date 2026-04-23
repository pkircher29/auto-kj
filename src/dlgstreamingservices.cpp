#include "dlgstreamingservices.h"
#include "ui_dlgstreamingservices.h"

#include "streaming/spotifycontroller.h"
#include "streaming/youtubemusiccontroller.h"
#include "streaming/itunescontroller.h"

#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QSettings>
#include <QMessageBox>
#include "settings.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString formatDuration(int secs)
{
    return QString("%1:%2")
        .arg(secs / 60)
        .arg(secs % 60, 2, 10, QChar('0'));
}

// ── Constructor ───────────────────────────────────────────────────────────────

DlgStreamingServices::DlgStreamingServices(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DlgStreamingServices)
    , m_spotify(new SpotifyController(this))
    , m_ytm(new YouTubeMusicController(this))
    , m_itunes(new ITunesController(this))
{
    ui->setupUi(this);
    setupSpotifyTab();
    setupYtmTab();
    setupItunesTab();

    // Restore saved settings
    QSettings s;
    Settings settings;
    ui->lineEditSpotifyClientId->setText(s.value("streaming/spotify_client_id").toString());
    ui->lineEditYtmClientId->setText(s.value("streaming/youtube_client_id").toString());
    ui->lineEditYtmApiKey->setText(settings.youtubeApiKey());

    connect(ui->btnSaveSettings, &QPushButton::clicked, this, &DlgStreamingServices::onSaveSettings);
}

DlgStreamingServices::~DlgStreamingServices()
{
    delete ui;
}

StreamingController *DlgStreamingServices::activeController() const
{
    if (m_spotify->isConnected())  return m_spotify;
    if (m_ytm->isConnected())      return m_ytm;
    if (m_itunes->isConnected())   return m_itunes;
    return nullptr;
}

// ── Tab setup ─────────────────────────────────────────────────────────────────

void DlgStreamingServices::setupSpotifyTab()
{
    connect(ui->btnSpotifyConnect,   &QPushButton::clicked,
            this, &DlgStreamingServices::onSpotifyConnect);
    connect(ui->btnSpotifyPlay, &QPushButton::clicked, this, &DlgStreamingServices::onSpotifyPlay);
    connect(ui->btnSpotifyNext, &QPushButton::clicked, this, &DlgStreamingServices::onSpotifyNext);
    connect(ui->btnSpotifyPrev, &QPushButton::clicked, this, &DlgStreamingServices::onSpotifyPrev);
    connect(ui->btnSpotifyHelp, &QPushButton::clicked, this, [this]() { showHelp(StreamingController::Service::Spotify); });

    connect(ui->listSpotifyPlaylists, &QListWidget::currentRowChanged,
            this, [this](int) { onSpotifyPlaylistSelected(); });
    connect(ui->tableSpotifyTracks, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onSpotifyTrackDoubleClicked(); });

    connect(m_spotify, &SpotifyController::connected,
            this, &DlgStreamingServices::onSpotifyConnected);
    connect(m_spotify, &SpotifyController::disconnected,
            this, &DlgStreamingServices::onSpotifyDisconnected);
    connect(m_spotify, &SpotifyController::playlistsLoaded,
            this, &DlgStreamingServices::onSpotifyPlaylistsLoaded);
    connect(m_spotify, &SpotifyController::tracksLoaded,
            this, &DlgStreamingServices::onSpotifyTracksLoaded);
    connect(m_spotify, &SpotifyController::nowPlayingChanged,
            this, &DlgStreamingServices::onSpotifyNowPlaying);
    connect(m_spotify, &SpotifyController::errorOccurred,
            this, [this](const QString &msg) {
                QMessageBox::warning(this, tr("Spotify"), msg);
            });
}

void DlgStreamingServices::setupYtmTab()
{
    connect(ui->btnYtmConnect, &QPushButton::clicked,
            this, &DlgStreamingServices::onYtmConnect);
    connect(ui->btnYtmPlay, &QPushButton::clicked, this, &DlgStreamingServices::onYtmPlay);
    connect(ui->btnYtmNext, &QPushButton::clicked, this, &DlgStreamingServices::onYtmNext);
    connect(ui->btnYtmPrev, &QPushButton::clicked, this, &DlgStreamingServices::onYtmPrev);
    connect(ui->btnYtmHelp, &QPushButton::clicked, this, [this]() { showHelp(StreamingController::Service::YouTubeMusic); });

    connect(ui->listYtmPlaylists, &QListWidget::currentRowChanged,
            this, [this](int) { onYtmPlaylistSelected(); });
    connect(ui->tableYtmTracks, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onYtmTrackDoubleClicked(); });

    connect(m_ytm, &YouTubeMusicController::connected,
            this, &DlgStreamingServices::onYtmConnected);
    connect(m_ytm, &YouTubeMusicController::disconnected,
            this, &DlgStreamingServices::onYtmDisconnected);
    connect(m_ytm, &YouTubeMusicController::playlistsLoaded,
            this, &DlgStreamingServices::onYtmPlaylistsLoaded);
    connect(m_ytm, &YouTubeMusicController::tracksLoaded,
            this, &DlgStreamingServices::onYtmTracksLoaded);
    connect(m_ytm, &YouTubeMusicController::nowPlayingChanged,
            this, &DlgStreamingServices::onYtmNowPlaying);
    connect(m_ytm, &YouTubeMusicController::errorOccurred,
            this, [this](const QString &msg) {
                QMessageBox::warning(this, tr("YouTube Music"), msg);
            });
}

void DlgStreamingServices::setupItunesTab()
{
    connect(ui->btnItunesConnect, &QPushButton::clicked,
            this, &DlgStreamingServices::onItunesConnect);
    connect(ui->btnItunesPlay, &QPushButton::clicked, this, &DlgStreamingServices::onItunesPlay);
    connect(ui->btnItunesNext, &QPushButton::clicked, this, &DlgStreamingServices::onItunesNext);
    connect(ui->btnItunesPrev, &QPushButton::clicked, this, &DlgStreamingServices::onItunesPrev);

    connect(ui->listItunesPlaylists, &QListWidget::currentRowChanged,
            this, [this](int) { onItunesPlaylistSelected(); });
    connect(ui->tableItunesTracks, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { onItunesTrackDoubleClicked(); });

    connect(m_itunes, &ITunesController::connected,
            this, &DlgStreamingServices::onItunesConnected);
    connect(m_itunes, &ITunesController::disconnected,
            this, &DlgStreamingServices::onItunesDisconnected);
    connect(m_itunes, &ITunesController::playlistsLoaded,
            this, &DlgStreamingServices::onItunesPlaylistsLoaded);
    connect(m_itunes, &ITunesController::tracksLoaded,
            this, &DlgStreamingServices::onItunesTracksLoaded);
    connect(m_itunes, &ITunesController::nowPlayingChanged,
            this, &DlgStreamingServices::onItunesNowPlaying);
    connect(m_itunes, &ITunesController::errorOccurred,
            this, [this](const QString &msg) {
                QMessageBox::warning(this, tr("iTunes"), msg);
            });

    // Relabel connect button per platform
    ui->btnItunesConnect->setText(tr("Launch %1").arg(m_itunes->serviceName()));
}

// ── Status badge ──────────────────────────────────────────────────────────────

void DlgStreamingServices::updateStatusBadge(QWidget *badge, bool connected)
{
    auto *lbl = qobject_cast<QLabel*>(badge);
    if (!lbl) return;
    if (connected) {
        lbl->setText(tr("● Connected"));
        lbl->setStyleSheet("color: #2ecc71; font-weight: bold;");
    } else {
        lbl->setText(tr("● Disconnected"));
        lbl->setStyleSheet("color: #e74c3c; font-weight: bold;");
    }
}

// ── Settings save ─────────────────────────────────────────────────────────────

void DlgStreamingServices::onSaveSettings()
{
    const QString clientId = ui->lineEditSpotifyClientId->text().trimmed();
    if (!clientId.isEmpty())
        m_spotify->setClientId(clientId);

    const QString ytmClientId = ui->lineEditYtmClientId->text().trimmed();
    if (!ytmClientId.isEmpty())
        m_ytm->setClientId(ytmClientId);

    m_ytm->setApiKey(ui->lineEditYtmApiKey->text().trimmed());
}

void DlgStreamingServices::showHelp(StreamingController::Service service)
{
    QString title, text;
    if (service == StreamingController::Service::Spotify) {
        title = tr("Spotify Setup Instructions");
        text = tr("<h3>How to get a Spotify Client ID:</h3>"
                  "<ol>"
                  "<li>Go to the <b>Spotify Developer Dashboard</b> (developer.spotify.com/dashboard).</li>"
                  "<li>Log in and click <b>Create app</b>.</li>"
                  "<li>Set the <b>App name</b> and <b>Description</b> (e.g., 'AutoKJ').</li>"
                  "<li>Under <b>Redirect URIs</b>, add: <code>http://localhost:8888/callback</code></li>"
                  "<li>Check the 'Web API' and 'Spotify Connect SDK' boxes if asked.</li>"
                  "<li>Save the app and copy the <b>Client ID</b> from the app settings page.</li>"
                  "<li>Paste the ID into AutoKJ and click <b>Connect</b>.</li>"
                  "</ol>");
    } else if (service == StreamingController::Service::YouTubeMusic) {
        title = tr("YouTube Music Setup Instructions");
        text = tr("<h3>How to get a Google Client ID:</h3>"
                  "<ol>"
                  "<li>Go to the <b>Google Cloud Console</b> (console.cloud.google.com).</li>"
                  "<li>Create a new project (e.g., 'AutoKJ-Streaming').</li>"
                  "<li>Go to <b>APIs & Services &gt; Library</b> and enable the <b>YouTube Data API v3</b>.</li>"
                  "<li>Go to <b>APIs & Services &gt; OAuth consent screen</b>, set it to 'External', and fill in basic info.</li>"
                  "<li>Go to <b>Credentials &gt; Create Credentials &gt; OAuth client ID</b>.</li>"
                  "<li>Select Application type: <b>Desktop app</b>.</li>"
                  "<li>Add Authorized redirect URI: <code>http://localhost:8889/callback</code></li>"
                  "<li>Copy the generated <b>Client ID</b> and paste it into AutoKJ.</li>"
                  "</ol>"
                  "<p><i>Note: An API Key is optional but helps with playlist quota limits.</i></p>");
    }

    QMessageBox::information(this, title, text);
}

// ══════════════════════════════  SPOTIFY SLOTS  ═══════════════════════════════

void DlgStreamingServices::onSpotifyConnect()
{
    if (m_spotify->isConnected()) {
        m_spotify->disconnectService();
    } else {
        const QString clientId = ui->lineEditSpotifyClientId->text().trimmed();
        if (!clientId.isEmpty())
            m_spotify->setClientId(clientId);
        m_spotify->connectService();
    }
}

void DlgStreamingServices::onSpotifyConnected()
{
    updateStatusBadge(ui->lblSpotifyStatus, true);
    ui->btnSpotifyConnect->setText(tr("Disconnect"));
}

void DlgStreamingServices::onSpotifyDisconnected(const QString &)
{
    updateStatusBadge(ui->lblSpotifyStatus, false);
    ui->btnSpotifyConnect->setText(tr("Connect"));
    ui->listSpotifyPlaylists->clear();
    ui->tableSpotifyTracks->setRowCount(0);
}

void DlgStreamingServices::onSpotifyPlaylistsLoaded(const QList<StreamPlaylist> &playlists)
{
    ui->listSpotifyPlaylists->clear();
    for (const auto &pl : playlists) {
        auto *item = new QListWidgetItem(
            QString("%1  (%2)").arg(pl.name).arg(pl.trackCount));
        item->setData(Qt::UserRole, QVariant::fromValue(pl.id));
        ui->listSpotifyPlaylists->addItem(item);
    }
}

void DlgStreamingServices::onSpotifyTracksLoaded(const QString &,
                                                  const QList<StreamTrack> &tracks)
{
    ui->tableSpotifyTracks->setRowCount(0);
    for (const auto &t : tracks) {
        const int row = ui->tableSpotifyTracks->rowCount();
        ui->tableSpotifyTracks->insertRow(row);
        ui->tableSpotifyTracks->setItem(row, 0, new QTableWidgetItem(t.artist));
        ui->tableSpotifyTracks->setItem(row, 1, new QTableWidgetItem(t.title));
        ui->tableSpotifyTracks->setItem(row, 2, new QTableWidgetItem(t.album));
        ui->tableSpotifyTracks->setItem(row, 3, new QTableWidgetItem(formatDuration(t.durationSecs)));
        // Store track URI in artist cell for retrieval
        ui->tableSpotifyTracks->item(row, 0)->setData(Qt::UserRole, t.playUrl);
        ui->tableSpotifyTracks->item(row, 0)->setData(Qt::UserRole + 1, t.id);
    }
    ui->tableSpotifyTracks->resizeColumnsToContents();
}

void DlgStreamingServices::onSpotifyPlaylistSelected()
{
    const auto *item = ui->listSpotifyPlaylists->currentItem();
    if (!item) return;
    m_currentSpotifyPlaylistId = item->data(Qt::UserRole).toString();
    StreamPlaylist pl;
    pl.id   = m_currentSpotifyPlaylistId;
    pl.name = item->text();
    m_spotify->loadTracks(pl);
}

void DlgStreamingServices::onSpotifyTrackDoubleClicked()
{
    const int row = ui->tableSpotifyTracks->currentRow();
    if (row < 0) return;
    StreamTrack t;
    t.playUrl = ui->tableSpotifyTracks->item(row, 0)->data(Qt::UserRole).toString();
    t.id      = ui->tableSpotifyTracks->item(row, 0)->data(Qt::UserRole + 1).toString();
    t.artist  = ui->tableSpotifyTracks->item(row, 0)->text();
    t.title   = ui->tableSpotifyTracks->item(row, 1)->text();
    m_spotify->playTrack(t);
}

void DlgStreamingServices::onSpotifyPlay()
{
    if (m_spotifyPlaying) m_spotify->pause();
    else                  m_spotify->resume();
    m_spotifyPlaying = !m_spotifyPlaying;
}

void DlgStreamingServices::onSpotifyNext() { m_spotify->next(); }
void DlgStreamingServices::onSpotifyPrev() { m_spotify->previous(); }

void DlgStreamingServices::onSpotifyNowPlaying(const StreamTrack &track)
{
    ui->lblSpotifyNowPlaying->setText(
        QString("%1 — %2").arg(track.artist, track.title));
    emit nowPlayingChanged(track);
}

// ═══════════════════════════  YOUTUBE MUSIC SLOTS  ════════════════════════════

void DlgStreamingServices::onYtmConnect()
{
    if (m_ytm->isConnected()) {
        m_ytm->disconnectService();
    } else {
        const QString clientId = ui->lineEditYtmClientId->text().trimmed();
        if (!clientId.isEmpty()) m_ytm->setClientId(clientId);
        const QString key = ui->lineEditYtmApiKey->text().trimmed();
        if (!key.isEmpty()) m_ytm->setApiKey(key);
        m_ytm->connectService();
    }
}

void DlgStreamingServices::onYtmConnected()
{
    updateStatusBadge(ui->lblYtmStatus, true);
    ui->btnYtmConnect->setText(tr("Disconnect"));
    m_ytm->loadPlaylists();
}

void DlgStreamingServices::onYtmDisconnected(const QString &)
{
    updateStatusBadge(ui->lblYtmStatus, false);
    ui->btnYtmConnect->setText(tr("Connect"));
    ui->listYtmPlaylists->clear();
    ui->tableYtmTracks->setRowCount(0);
}

void DlgStreamingServices::onYtmPlaylistsLoaded(const QList<StreamPlaylist> &playlists)
{
    ui->listYtmPlaylists->clear();
    for (const auto &pl : playlists) {
        auto *item = new QListWidgetItem(
            QString("%1  (%2)").arg(pl.name).arg(pl.trackCount));
        item->setData(Qt::UserRole, QVariant::fromValue(pl.id));
        ui->listYtmPlaylists->addItem(item);
    }
}

void DlgStreamingServices::onYtmTracksLoaded(const QString &,
                                              const QList<StreamTrack> &tracks)
{
    ui->tableYtmTracks->setRowCount(0);
    for (const auto &t : tracks) {
        const int row = ui->tableYtmTracks->rowCount();
        ui->tableYtmTracks->insertRow(row);
        ui->tableYtmTracks->setItem(row, 0, new QTableWidgetItem(t.artist));
        ui->tableYtmTracks->setItem(row, 1, new QTableWidgetItem(t.title));
        ui->tableYtmTracks->item(row, 0)->setData(Qt::UserRole,     t.playUrl);
        ui->tableYtmTracks->item(row, 0)->setData(Qt::UserRole + 1, t.id);
    }
    ui->tableYtmTracks->resizeColumnsToContents();
}

void DlgStreamingServices::onYtmPlaylistSelected()
{
    const auto *item = ui->listYtmPlaylists->currentItem();
    if (!item) return;
    m_currentYtmPlaylistId = item->data(Qt::UserRole).toString();
    StreamPlaylist pl;
    pl.id   = m_currentYtmPlaylistId;
    pl.name = item->text();
    m_ytm->loadTracks(pl);
}

void DlgStreamingServices::onYtmTrackDoubleClicked()
{
    const int row = ui->tableYtmTracks->currentRow();
    if (row < 0) return;
    StreamTrack t;
    t.playUrl = ui->tableYtmTracks->item(row, 0)->data(Qt::UserRole).toString();
    t.id      = ui->tableYtmTracks->item(row, 0)->data(Qt::UserRole + 1).toString();
    t.artist  = ui->tableYtmTracks->item(row, 0)->text();
    t.title   = ui->tableYtmTracks->item(row, 1)->text();
    m_ytm->playTrack(t);
}

void DlgStreamingServices::onYtmPlay()
{
    if (m_ytmPlaying) m_ytm->pause();
    else              m_ytm->resume();
    m_ytmPlaying = !m_ytmPlaying;
}

void DlgStreamingServices::onYtmNext() { m_ytm->next(); }
void DlgStreamingServices::onYtmPrev() { m_ytm->previous(); }

void DlgStreamingServices::onYtmNowPlaying(const StreamTrack &track)
{
    ui->lblYtmNowPlaying->setText(
        QString("%1 — %2").arg(track.artist, track.title));
    emit nowPlayingChanged(track);
}

// ═════════════════════════════  ITUNES SLOTS  ═════════════════════════════════

void DlgStreamingServices::onItunesConnect()
{
    if (m_itunes->isConnected()) {
        m_itunes->disconnectService();
    } else {
        m_itunes->connectService();
    }
}

void DlgStreamingServices::onItunesConnected()
{
    updateStatusBadge(ui->lblItunesStatus, true);
    ui->btnItunesConnect->setText(tr("Disconnect"));
}

void DlgStreamingServices::onItunesDisconnected(const QString &)
{
    updateStatusBadge(ui->lblItunesStatus, false);
    ui->btnItunesConnect->setText(tr("Launch %1").arg(m_itunes->serviceName()));
    ui->listItunesPlaylists->clear();
    ui->tableItunesTracks->setRowCount(0);
}

void DlgStreamingServices::onItunesPlaylistsLoaded(const QList<StreamPlaylist> &playlists)
{
    ui->listItunesPlaylists->clear();
    for (const auto &pl : playlists) {
        auto *item = new QListWidgetItem(
            QString("%1  (%2)").arg(pl.name).arg(pl.trackCount));
        item->setData(Qt::UserRole, QVariant::fromValue(pl.id));
        ui->listItunesPlaylists->addItem(item);
    }
}

void DlgStreamingServices::onItunesTracksLoaded(const QString &,
                                                 const QList<StreamTrack> &tracks)
{
    ui->tableItunesTracks->setRowCount(0);
    for (const auto &t : tracks) {
        const int row = ui->tableItunesTracks->rowCount();
        ui->tableItunesTracks->insertRow(row);
        ui->tableItunesTracks->setItem(row, 0, new QTableWidgetItem(t.artist));
        ui->tableItunesTracks->setItem(row, 1, new QTableWidgetItem(t.title));
        ui->tableItunesTracks->setItem(row, 2, new QTableWidgetItem(t.album));
        ui->tableItunesTracks->setItem(row, 3, new QTableWidgetItem(formatDuration(t.durationSecs)));
        ui->tableItunesTracks->item(row, 0)->setData(Qt::UserRole,     t.playUrl);
        ui->tableItunesTracks->item(row, 0)->setData(Qt::UserRole + 1, t.id);
    }
    ui->tableItunesTracks->resizeColumnsToContents();
}

void DlgStreamingServices::onItunesPlaylistSelected()
{
    const auto *item = ui->listItunesPlaylists->currentItem();
    if (!item) return;
    m_currentItunesPlaylistId = item->data(Qt::UserRole).toString();
    StreamPlaylist pl;
    pl.id   = m_currentItunesPlaylistId;
    pl.name = item->text();
    m_itunes->loadTracks(pl);
}

void DlgStreamingServices::onItunesTrackDoubleClicked()
{
    const int row = ui->tableItunesTracks->currentRow();
    if (row < 0) return;
    StreamTrack t;
    t.playUrl = ui->tableItunesTracks->item(row, 0)->data(Qt::UserRole).toString();
    t.id      = ui->tableItunesTracks->item(row, 0)->data(Qt::UserRole + 1).toString();
    t.artist  = ui->tableItunesTracks->item(row, 0)->text();
    t.title   = ui->tableItunesTracks->item(row, 1)->text();
    m_itunes->playTrack(t);
}

void DlgStreamingServices::onItunesPlay()
{
    if (m_itunesPlaying) m_itunes->pause();
    else                 m_itunes->resume();
    m_itunesPlaying = !m_itunesPlaying;
}

void DlgStreamingServices::onItunesNext() { m_itunes->next(); }
void DlgStreamingServices::onItunesPrev() { m_itunes->previous(); }

void DlgStreamingServices::onItunesNowPlaying(const StreamTrack &track)
{
    ui->lblItunesNowPlaying->setText(
        QString("%1 — %2").arg(track.artist, track.title));
    emit nowPlayingChanged(track);
}
