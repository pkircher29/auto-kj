#pragma once

#include <QObject>
#include <QString>
#include <QList>

/*
 * StreamingController — abstract base for Spotify, YouTube Music, and iTunes
 * wrappers. Each implementation controls the service's own application running
 * in the background; audio is routed by the OS, not through GStreamer.
 */

struct StreamTrack {
    QString id;           // service-specific track ID or URI
    QString title;
    QString artist;
    QString album;
    QString playUrl;      // URL / URI used to trigger playback of this track
    int     durationSecs{0};
};

struct StreamPlaylist {
    QString id;
    QString name;
    QString description;
    int     trackCount{0};
    QString imageUrl;
};

class StreamingController : public QObject
{
    Q_OBJECT
public:
    enum class Service { Spotify, YouTubeMusic, iTunes };

    explicit StreamingController(QObject *parent = nullptr) : QObject(parent) {}

    virtual Service  service()     const = 0;
    virtual QString  serviceName() const = 0;
    virtual bool     isConnected() const = 0;

    // Auth / lifecycle
    virtual void connectService()    = 0;
    virtual void disconnectService() = 0;

    // Browsing
    virtual void loadPlaylists()                                = 0;
    virtual void loadTracks(const StreamPlaylist &playlist)     = 0;

    // Playback control
    virtual void playPlaylist(const StreamPlaylist &playlist)   = 0;
    virtual void playTrack(const StreamTrack &track)            = 0;
    virtual void pause()                                        = 0;
    virtual void resume()                                       = 0;
    virtual void next()                                         = 0;
    virtual void previous()                                     = 0;
    virtual void setVolume(int percent)                         = 0;

signals:
    void connected();
    void disconnected(const QString &reason);
    void playlistsLoaded(const QList<StreamPlaylist> &playlists);
    void tracksLoaded(const QString &playlistId, const QList<StreamTrack> &tracks);
    void nowPlayingChanged(const StreamTrack &track);
    void playbackStateChanged(bool isPlaying);
    void errorOccurred(const QString &message);
};
