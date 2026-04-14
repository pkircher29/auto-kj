#pragma once

#include <QDialog>
#include <QList>
#include "streaming/streamingcontroller.h"

namespace Ui { class DlgStreamingServices; }
class SpotifyController;
class YouTubeMusicController;
class ITunesController;

/*
 * DlgStreamingServices — tabbed dialog for Spotify, YouTube Music, and iTunes.
 *
 * Each tab shows:
 *   - Connect / Disconnect button with status indicator
 *   - Playlist list (left)
 *   - Track list (right, populated on playlist selection)
 *   - Transport bar (prev / play-pause / next)
 *   - Now-playing label at the bottom
 */

class DlgStreamingServices : public QDialog
{
    Q_OBJECT
public:
    explicit DlgStreamingServices(QWidget *parent = nullptr);
    ~DlgStreamingServices() override;

    // Returns the currently active controller (nullptr if none connected).
    StreamingController *activeController() const;

signals:
    void nowPlayingChanged(const StreamTrack &track);
    void playbackStateChanged(bool playing);

private slots:
    // Spotify tab
    void onSpotifyConnect();
    void onSpotifyConnected();
    void onSpotifyDisconnected(const QString &reason);
    void onSpotifyPlaylistsLoaded(const QList<StreamPlaylist> &playlists);
    void onSpotifyTracksLoaded(const QString &playlistId, const QList<StreamTrack> &tracks);
    void onSpotifyPlaylistSelected();
    void onSpotifyTrackDoubleClicked();
    void onSpotifyPlay();
    void onSpotifyNext();
    void onSpotifyPrev();
    void onSpotifyNowPlaying(const StreamTrack &track);

    // YouTube Music tab
    void onYtmConnect();
    void onYtmConnected();
    void onYtmDisconnected(const QString &reason);
    void onYtmPlaylistsLoaded(const QList<StreamPlaylist> &playlists);
    void onYtmTracksLoaded(const QString &playlistId, const QList<StreamTrack> &tracks);
    void onYtmPlaylistSelected();
    void onYtmTrackDoubleClicked();
    void onYtmPlay();
    void onYtmNext();
    void onYtmPrev();
    void onYtmNowPlaying(const StreamTrack &track);

    // iTunes tab
    void onItunesConnect();
    void onItunesConnected();
    void onItunesDisconnected(const QString &reason);
    void onItunesPlaylistsLoaded(const QList<StreamPlaylist> &playlists);
    void onItunesTracksLoaded(const QString &playlistId, const QList<StreamTrack> &tracks);
    void onItunesPlaylistSelected();
    void onItunesTrackDoubleClicked();
    void onItunesPlay();
    void onItunesNext();
    void onItunesPrev();
    void onItunesNowPlaying(const StreamTrack &track);

    // Settings
    void onSaveSettings();

private:
    void setupSpotifyTab();
    void setupYtmTab();
    void setupItunesTab();
    void updateStatusBadge(QWidget *badge, bool connected);
    void showHelp(StreamingController::Service service);

    Ui::DlgStreamingServices *ui;

    SpotifyController       *m_spotify{nullptr};
    YouTubeMusicController  *m_ytm{nullptr};
    ITunesController        *m_itunes{nullptr};

    bool m_spotifyPlaying{false};
    bool m_ytmPlaying{false};
    bool m_itunesPlaying{false};

    QString m_currentSpotifyPlaylistId;
    QString m_currentYtmPlaylistId;
    QString m_currentItunesPlaylistId;
};
