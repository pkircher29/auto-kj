#include "itunescontroller.h"

#include <QProcess>
#include <QStringList>

#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
#  include <QAxObject>
#endif

#if defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
#  include <QDBusInterface>
#  include <QDBusReply>
#endif

// ── Service name ──────────────────────────────────────────────────────────────

QString ITunesController::serviceName() const
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("Apple Music");
#else
    return QStringLiteral("iTunes");
#endif
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

ITunesController::ITunesController(QObject *parent)
    : StreamingController(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &ITunesController::onPollTimer);
}

ITunesController::~ITunesController()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    delete m_itunes;
#endif
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ITunesController::connectService()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    ensureRunning();
    if (m_itunes) {
        m_connected = true;
        m_pollTimer->start();
        emit connected();
        loadPlaylists();
    }
#elif defined(Q_OS_MACOS)
    // Launch Music.app if not running
    runScript("tell application \"Music\" to activate");
    m_connected = true;
    m_pollTimer->start();
    emit connected();
    loadPlaylists();
#else
    // Linux: assume an MPRIS-compatible player is running
    m_connected = true;
    m_pollTimer->start();
    emit connected();
    loadPlaylists();
#endif
}

void ITunesController::disconnectService()
{
    m_connected = false;
    m_pollTimer->stop();
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    delete m_itunes;
    m_itunes = nullptr;
#endif
    emit disconnected({});
}

// ── Windows COM helpers ───────────────────────────────────────────────────────

#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
void ITunesController::ensureRunning()
{
    if (!m_itunes) {
        m_itunes = new QAxObject("iTunes.Application", this);
        if (m_itunes->isNull()) {
            delete m_itunes;
            m_itunes = nullptr;
            emit errorOccurred(tr("iTunes is not installed or could not be launched."));
        }
    }
}

void ITunesController::axCall(const QString &method, const QList<QVariant> &args)
{
    if (!m_itunes) return;
    m_itunes->dynamicCall(method.toLocal8Bit().constData(),
                          const_cast<QList<QVariant>&>(args));
}

QVariant ITunesController::axGet(const QString &property)
{
    if (!m_itunes) return {};
    return m_itunes->property(property.toLocal8Bit().constData());
}
#endif

// ── macOS AppleScript helper ──────────────────────────────────────────────────

QString ITunesController::runScript(const QString &script) const
{
#if defined(Q_OS_MACOS)
    QProcess proc;
    proc.start("osascript", {"-e", script});
    proc.waitForFinished(5000);
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
#else
    Q_UNUSED(script)
    return {};
#endif
}

// ── Browsing ──────────────────────────────────────────────────────────────────

void ITunesController::loadPlaylists()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    ensureRunning();
    if (!m_itunes) return;

    QAxObject *library = m_itunes->querySubObject("LibrarySource");
    if (!library) return;
    QAxObject *plLists = library->querySubObject("Playlists");
    if (!plLists) return;
    int count = plLists->property("Count").toInt();

    QList<StreamPlaylist> playlists;
    for (int i = 1; i <= count; ++i) {
        QAxObject *pl = plLists->querySubObject("Item(int)", i);
        if (!pl) continue;
        StreamPlaylist p;
        p.id         = QString::number(pl->property("playlistID").toInt());
        p.name       = pl->property("Name").toString();
        p.trackCount = pl->property("Tracks").toInt();
        playlists.append(p);
        delete pl;
    }
    delete plLists;
    delete library;
    emit playlistsLoaded(playlists);

#elif defined(Q_OS_MACOS)
    const QString raw = runScript(
        "tell application \"Music\"\n"
        "  set result to \"\"\n"
        "  repeat with pl in playlists\n"
        "    set result to result & (get persistent ID of pl) & \"|\" "
        "& (get name of pl) & \"|\" & (count of tracks of pl) & \"\\n\"\n"
        "  end repeat\n"
        "  return result\n"
        "end tell"
    );
    QList<StreamPlaylist> playlists;
    for (const auto &line : raw.split('\n', Qt::SkipEmptyParts)) {
        const QStringList parts = line.split('|');
        if (parts.size() < 2) continue;
        StreamPlaylist pl;
        pl.id         = parts[0].trimmed();
        pl.name       = parts[1].trimmed();
        pl.trackCount = parts.value(2).trimmed().toInt();
        playlists.append(pl);
    }
    emit playlistsLoaded(playlists);

#elif defined(Q_OS_LINUX)
    // Enumerate MPRIS playlists (optional extension; most players don't support it)
    // Fall back to a single "Library" playlist entry
    QList<StreamPlaylist> playlists;
    StreamPlaylist lib;
    lib.id   = "library";
    lib.name = tr("Library");
    playlists.append(lib);
    emit playlistsLoaded(playlists);
#endif
}

void ITunesController::loadTracks(const StreamPlaylist &playlist)
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    ensureRunning();
    if (!m_itunes) return;

    QAxObject *library = m_itunes->querySubObject("LibrarySource");
    if (!library) return;
    QAxObject *plLists = library->querySubObject("Playlists");
    if (!plLists) return;

    // Find playlist by ID
    const int targetId = playlist.id.toInt();
    int count = plLists->property("Count").toInt();
    QAxObject *targetPl = nullptr;
    for (int i = 1; i <= count; ++i) {
        QAxObject *pl = plLists->querySubObject("Item(int)", i);
        if (!pl) continue;
        if (pl->property("playlistID").toInt() == targetId) {
            targetPl = pl;
            break;
        }
        delete pl;
    }
    delete plLists;
    delete library;
    if (!targetPl) return;

    QAxObject *tracks = targetPl->querySubObject("Tracks");
    delete targetPl;
    if (!tracks) return;

    int tCount = tracks->property("Count").toInt();
    QList<StreamTrack> trackList;
    for (int i = 1; i <= tCount; ++i) {
        QAxObject *tr = tracks->querySubObject("Item(int)", i);
        if (!tr) continue;
        StreamTrack t;
        t.id           = QString::number(tr->property("TrackDatabaseID").toInt());
        t.title        = tr->property("Name").toString();
        t.artist       = tr->property("Artist").toString();
        t.album        = tr->property("Album").toString();
        t.durationSecs = tr->property("Duration").toInt();
        t.playUrl      = tr->property("Location").toString(); // file:// path
        trackList.append(t);
        delete tr;
    }
    delete tracks;
    emit tracksLoaded(playlist.id, trackList);

#elif defined(Q_OS_MACOS)
    const QString script = QString(
        "tell application \"Music\"\n"
        "  set pl to (first playlist whose persistent ID is \"%1\")\n"
        "  set result to \"\"\n"
        "  repeat with tr in tracks of pl\n"
        "    set result to result & (get persistent ID of tr) & \"|\" "
        "& (get name of tr) & \"|\" & (get artist of tr) & \"|\" "
        "& (get album of tr) & \"|\" & (get duration of tr) & \"\\n\"\n"
        "  end repeat\n"
        "  return result\n"
        "end tell"
    ).arg(playlist.id);
    const QString raw = runScript(script);
    QList<StreamTrack> tracks;
    for (const auto &line : raw.split('\n', Qt::SkipEmptyParts)) {
        const QStringList p = line.split('|');
        if (p.size() < 4) continue;
        StreamTrack t;
        t.id           = p[0].trimmed();
        t.title        = p[1].trimmed();
        t.artist       = p[2].trimmed();
        t.album        = p[3].trimmed();
        t.durationSecs = static_cast<int>(p.value(4).trimmed().toDouble());
        t.playUrl      = t.id;  // use persistent ID for play commands
        tracks.append(t);
    }
    emit tracksLoaded(playlist.id, tracks);

#else
    Q_UNUSED(playlist)
    emit tracksLoaded(playlist.id, {});
#endif
}

// ── Playback ──────────────────────────────────────────────────────────────────

void ITunesController::playPlaylist(const StreamPlaylist &playlist)
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    ensureRunning();
    if (!m_itunes) return;

    QAxObject *library = m_itunes->querySubObject("LibrarySource");
    QAxObject *pls     = library ? library->querySubObject("Playlists") : nullptr;
    if (!pls) { delete library; return; }

    const int targetId = playlist.id.toInt();
    int count = pls->property("Count").toInt();
    for (int i = 1; i <= count; ++i) {
        QAxObject *pl = pls->querySubObject("Item(int)", i);
        if (!pl) continue;
        if (pl->property("playlistID").toInt() == targetId) {
            pl->dynamicCall("PlayFirstTrack()");
            delete pl;
            break;
        }
        delete pl;
    }
    delete pls;
    delete library;

#elif defined(Q_OS_MACOS)
    runScript(QString(
        "tell application \"Music\"\n"
        "  set pl to (first playlist whose persistent ID is \"%1\")\n"
        "  play pl\n"
        "end tell"
    ).arg(playlist.id));
#endif
}

void ITunesController::playTrack(const StreamTrack &track)
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    ensureRunning();
    if (!m_itunes) return;
    // Use TrackDatabaseID search
    QAxObject *search = m_itunes->querySubObject(
        "LibraryPlaylist().Search(QString,long)", track.title, 1 /*kSearchSongsAndAlbums*/);
    if (!search) return;
    int count = search->property("Count").toInt();
    for (int i = 1; i <= count; ++i) {
        QAxObject *tr = search->querySubObject("Item(int)", i);
        if (!tr) continue;
        if (tr->property("TrackDatabaseID").toInt() == track.id.toInt()) {
            tr->dynamicCall("Play()");
            delete tr;
            break;
        }
        delete tr;
    }
    delete search;

#elif defined(Q_OS_MACOS)
    runScript(QString(
        "tell application \"Music\"\n"
        "  set tr to (first track of library playlist 1 whose persistent ID is \"%1\")\n"
        "  play tr\n"
        "end tell"
    ).arg(track.playUrl));
#endif
}

void ITunesController::pause()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    axCall("Pause()");
#elif defined(Q_OS_MACOS)
    runScript("tell application \"Music\" to pause");
#elif defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    QDBusInterface("org.mpris.MediaPlayer2.rhythmbox",
                   "/org/mpris/MediaPlayer2",
                   "org.mpris.MediaPlayer2.Player",
                   QDBusConnection::sessionBus()).call("Pause");
#endif
}

void ITunesController::resume()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    axCall("Play()");
#elif defined(Q_OS_MACOS)
    runScript("tell application \"Music\" to play");
#elif defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    QDBusInterface("org.mpris.MediaPlayer2.rhythmbox",
                   "/org/mpris/MediaPlayer2",
                   "org.mpris.MediaPlayer2.Player",
                   QDBusConnection::sessionBus()).call("Play");
#endif
}

void ITunesController::next()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    axCall("NextTrack()");
#elif defined(Q_OS_MACOS)
    runScript("tell application \"Music\" to next track");
#elif defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    QDBusInterface("org.mpris.MediaPlayer2.rhythmbox",
                   "/org/mpris/MediaPlayer2",
                   "org.mpris.MediaPlayer2.Player",
                   QDBusConnection::sessionBus()).call("Next");
#endif
}

void ITunesController::previous()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    axCall("PreviousTrack()");
#elif defined(Q_OS_MACOS)
    runScript("tell application \"Music\" to previous track");
#elif defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    QDBusInterface("org.mpris.MediaPlayer2.rhythmbox",
                   "/org/mpris/MediaPlayer2",
                   "org.mpris.MediaPlayer2.Player",
                   QDBusConnection::sessionBus()).call("Previous");
#endif
}

void ITunesController::setVolume(int percent)
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    axCall("set SoundVolume =", {percent});
#elif defined(Q_OS_MACOS)
    runScript(QString("tell application \"Music\" to set sound volume to %1").arg(percent));
#else
    Q_UNUSED(percent)
#endif
}

// ── Now-playing polling ───────────────────────────────────────────────────────

void ITunesController::onPollTimer()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    pollNowPlayingWindows();
#elif defined(Q_OS_MACOS)
    pollNowPlayingMac();
#elif defined(Q_OS_LINUX)
    pollNowPlayingLinux();
#endif
}

void ITunesController::pollNowPlayingWindows()
{
#if defined(Q_OS_WIN) && defined(HAVE_QT_AXCONTAINER)
    if (!m_itunes) return;
    QAxObject *current = m_itunes->querySubObject("CurrentTrack");
    if (!current) return;
    StreamTrack t;
    t.title        = current->property("Name").toString();
    t.artist       = current->property("Artist").toString();
    t.album        = current->property("Album").toString();
    t.durationSecs = current->property("Duration").toInt();
    delete current;
    if (!t.title.isEmpty()) {
        emit nowPlayingChanged(t);
        const int state = axGet("PlayerState").toInt();
        emit playbackStateChanged(state == 1 /*ITPlayerStatePlaying*/);
    }
#endif
}

void ITunesController::pollNowPlayingMac()
{
#if defined(Q_OS_MACOS)
    const QString raw = runScript(
        "tell application \"Music\"\n"
        "  if player state is playing then\n"
        "    set tr to current track\n"
        "    return (name of tr) & \"|\" & (artist of tr) & \"|\" "
        "& (album of tr) & \"|\" & (duration of tr) & \"|playing\"\n"
        "  else\n"
        "    return \"stopped\"\n"
        "  end if\n"
        "end tell"
    );
    if (raw == "stopped" || raw.isEmpty()) {
        emit playbackStateChanged(false);
        return;
    }
    const QStringList parts = raw.split('|');
    StreamTrack t;
    t.title        = parts.value(0).trimmed();
    t.artist       = parts.value(1).trimmed();
    t.album        = parts.value(2).trimmed();
    t.durationSecs = static_cast<int>(parts.value(3).trimmed().toDouble());
    if (!t.title.isEmpty())
        emit nowPlayingChanged(t);
    emit playbackStateChanged(parts.value(4).trimmed() == "playing");
#endif
}

void ITunesController::pollNowPlayingLinux()
{
#if defined(Q_OS_LINUX) && defined(HAVE_QT_DBUS)
    const QStringList candidates = {
        "org.mpris.MediaPlayer2.rhythmbox",
        "org.mpris.MediaPlayer2.banshee",
        "org.mpris.MediaPlayer2.clementine",
    };
    for (const auto &svc : candidates) {
        QDBusInterface iface(svc, "/org/mpris/MediaPlayer2",
                             "org.mpris.MediaPlayer2.Player",
                             QDBusConnection::sessionBus());
        if (!iface.isValid()) continue;
        QDBusReply<QVariant> meta =
            iface.call("org.freedesktop.DBus.Properties.Get",
                       "org.mpris.MediaPlayer2.Player", "Metadata");
        if (meta.isValid()) {
            QVariantMap map = meta.value().toMap();
            StreamTrack t;
            t.title  = map["xesam:title"].toString();
            t.artist = map["xesam:artist"].toStringList().join(", ");
            if (!t.title.isEmpty())
                emit nowPlayingChanged(t);
        }
        break;
    }
#endif
}
