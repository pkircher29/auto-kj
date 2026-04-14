#pragma once

#include "streamingcontroller.h"
#include <QTimer>
#include <QProcess>

#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
class QAxObject;
#endif

/*
 * ITunesController — controls iTunes / Apple Music in the background.
 *
 *   Windows : Qt ActiveX (QAxObject) — requires Qt5AxContainer.
 *             iTunes must be installed. COM prog-id: "iTunes.Application"
 *   macOS   : osascript / AppleScript targeting the "Music" application
 *             (renamed from iTunes in macOS 10.15+).
 *   Linux   : MPRIS2 D-Bus targeting rhythmbox / banshee / clementine if
 *             the user has their library imported there. Falls back to a
 *             GNOME-Music MPRIS endpoint. iTunes does not run on Linux.
 *
 * Library loading reads playlists from the running application; no XML
 * file is parsed so the view always reflects the live library state.
 */

class ITunesController : public StreamingController
{
    Q_OBJECT
public:
    explicit ITunesController(QObject *parent = nullptr);
    ~ITunesController() override;

    Service service()     const override { return Service::iTunes; }
    QString serviceName() const override;   // "iTunes" or "Apple Music"
    bool    isConnected() const override { return m_connected; }

    void connectService()    override;
    void disconnectService() override;

    void loadPlaylists()                             override;
    void loadTracks(const StreamPlaylist &playlist)  override;

    void playPlaylist(const StreamPlaylist &playlist) override;
    void playTrack(const StreamTrack &track)          override;
    void pause()    override;
    void resume()   override;
    void next()     override;
    void previous() override;
    void setVolume(int percent) override;

private slots:
    void onPollTimer();

private:
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    QAxObject *m_itunes{nullptr};
    void ensureRunning();
    void axCall(const QString &method, const QList<QVariant> &args = {});
    QVariant axGet(const QString &property);
#endif

    QString runScript(const QString &script) const;  // macOS osascript helper

    void pollNowPlayingMac();
    void pollNowPlayingLinux();
    void pollNowPlayingWindows();

    QTimer *m_pollTimer{nullptr};
    bool    m_connected{false};

    static constexpr int kPollIntervalMs = 4000;
};
