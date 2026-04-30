/*
 * Copyright (c) 2013-2019 Thomas Isaac Lightburn
 *
 *
 * This file is part of Auto-KJ.
 *
 * Auto-KJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QHeaderView>
#include <QObject>
#include <QSettings>
#include <QSplitter>
#include <QTableView>
#include <QTreeView>
#include <QWidget>
#include <QMetaType>
#include <QKeySequence>

struct SfxEntry
{
    SfxEntry();
    QString name;
    QString path;


}; Q_DECLARE_METATYPE(SfxEntry)


typedef QList<SfxEntry> SfxEntryList;

Q_DECLARE_METATYPE(QList<SfxEntry>)

class Settings : public QObject
{
    Q_OBJECT

private:
    QSettings *settings;
    bool m_safeStartupMode{false};

public:
    enum {
        LOG_LEVEL_DISABLED,
        LOG_LEVEL_CRITICAL,
        LOG_LEVEL_ERROR,
        LOG_LEVEL_WARNING,
        LOG_LEVEL_INFO,
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_TRACE
    };
    int getConsoleLogLevel();
    int getFileLogLevel();
    bool tickerReducedCpuMode();
    void setTickerReducedCpuMode(bool enabled);
    void setConsoleLogLevel(int level);
    void setFileLogLevel(int level);
    int lastRunRotationTopSingerId();
    void setLastRunRotationTopSingerId(int id);
    [[nodiscard]] bool lastStartupOk() const;
    void setStartupOk(bool ok);
    [[nodiscard]] QString lastRunVersion() const;
    void setLastRunVersion(const QString &version);
    [[nodiscard]] bool safeStartupMode() const;
    void setSafeStartupMode(bool safeMode);
    [[nodiscard]] int historyDblClickAction() const;
    void setHistoryDblClickAction(int index);
    int getSystemRamSize();
    int remainRtOffset();
    int remainBtmOffset();
    qint64 hash(const QString & str);
    bool progressiveSearchEnabled();
    QString storeDownloadDir();
    QString logDir();
    bool logShow();
    bool logEnabled();
    void setPassword(QString password);
    void clearPassword();
    bool chkPassword(QString password);
    bool passIsSet();
    // REMOVED: setCC, setSaveCC, saveCC, clearCC — credit card storage removed, use Stripe
    // REMOVED: getCCN/getCCM/getCCY/getCCV — credit card retrieval removed, use Stripe
    // REMOVED: clearKNAccount, setSaveKNAccount, saveKNAccount — karaokeDotNet removed
    // REMOVED: setKaroakeDotNetUser, setKaraokeDotNetPass, karoakeDotNetUser, karoakeDotNetPass
    bool testingEnabled();
    bool hardwareAccelEnabled();
    bool crashReportingEnabled();
    void setCrashReportingEnabled(bool enabled);
    bool dbDoubleClickAddsSong();
    enum BgMode { BG_MODE_IMAGE = 0, BG_MODE_SLIDESHOW };
    enum PreviewSize { Small, Medium, Large };
    explicit Settings(QObject *parent = 0);
    bool cdgWindowFullscreen();
    bool showCdgWindow();
    void setCdgWindowFullscreenMonitor(int monitor);
    int  cdgWindowFullScreenMonitor();
    void saveWindowState(QWidget *window);
    void restoreWindowState(QWidget *window);
    void saveColumnWidths(QTreeView *treeView);
    void saveColumnWidths(QTableView *tableView);
    void restoreColumnWidths(QTreeView *treeView);
    bool restoreColumnWidths(QTableView *tableView);
    void saveSplitterState(QSplitter *splitter);
    void restoreSplitterState(QSplitter *splitter);
    void setTickerFont(const QFont &font);
    void setApplicationFont(const QFont &font);
    QFont tickerFont();
    [[nodiscard]] QFont applicationFont() const;
    int tickerHeight();
    void setTickerHeight(int height);
    int tickerSpeed();
    void setTickerSpeed(int speed);
    QColor tickerTextColor();
    void setTickerTextColor(QColor color);
    bool cdgRemainEnabled();
    QColor tickerBgColor();
    void setTickerBgColor(QColor color);
    bool tickerFullRotation();
    void setTickerFullRotation(bool full);
    int tickerShowNumSingers();
    void setTickerShowNumSingers(int limit);
    void setTickerEnabled(bool enable);
    bool tickerEnabled();
    bool newSingerAlertsEnabled();
    void setNewSingerAlertsEnabled(bool enable);
    QString tickerCustomString();
    void setTickerCustomString(const QString &value);
    bool tickerShowRotationInfo();
    bool requestServerEnabled();
    void setRequestServerEnabled(bool enable);
    QString requestServerUrl() const;
    void setRequestServerUrl(QString url);
    int requestServerVenue();
    void setRequestServerVenue(int venueId);
    QString requestServerEmail() const;
    void setRequestServerEmail(const QString &email);
    QString requestServerPassword() const;
    void setRequestServerPassword(const QString &password);
    QString requestServerToken() const;
    void setRequestServerToken(const QString &token);
    QString requestServerApiKey() const;
    void setRequestServerApiKey(const QString &apiKey);
    QString requestServerSubscriptionTier() const;
    void setRequestServerSubscriptionTier(const QString &tier);
    bool requestServerIgnoreCertErrors();
    void setRequestServerIgnoreCertErrors(bool ignore);
    // AutoKJ Server settings (venue slug replaces numeric venue ID; token replaces API key)
    QString requestServerVenueSlug() const;
    void setRequestServerVenueSlug(const QString &slug);
    QString venueShowtimesJson(int venueId) const;
    void setVenueShowtimesJson(int venueId, const QString &json);
    // KJ name for analytics
    QString kjName();
    void setKjName(const QString &name);
    // DJ Profiles
    QStringList djList() const;
    void setDjList(const QStringList &djs);
    QStringList mediaDirs() const;
    void setMediaDirs(const QStringList &dirs);
    QString activeDj() const;
    void setActiveDj(const QString &dj);
    // Per-singer queue limit (enforced locally in addition to server-side)
    int maxQueuePerSinger() const;
    void setMaxQueuePerSinger(int limit);
    // Venue details
    QString venueAddress() const;
    double venueLat() const;
    double venueLon() const;
    double venueGeofenceRadius() const;
    bool venueGeofenceEnabled() const;
    // kjPin() removed — use venue slug or geolocation instead
    // Gig settings
    bool blockRepeatSongs() const;
    bool limitOneSongPerSinger() const;
    int kjBufferSeconds() const;
    int noShowTimeoutSeconds() const;
    bool lineJumpEnabled() const;
    // Max singers per request (solo, duet, etc. - default 4)
    int maxSingersPerRequest() const;
    void setMaxSingersPerRequest(int max);
    // Rotation style (0=Classic, 1=Double, 2=Flex)
    int rotationStyle() const;
    void setRotationStyle(int style);

    // Fairness engine settings
    bool fairnessEnabled() const;
    void setFairnessEnabled(bool enabled);
    // Effective premium gate for anti-chaos features (requires live server authorization)
    bool antiChaosPremiumEnabled() const;
    bool premiumAntiChaosAuthorized() const;
    void setPremiumAntiChaosAuthorized(bool authorized);
    bool fairnessEnforceRound() const;
    void setFairnessEnforceRound(bool enabled);
    bool fairnessEnforceSongNight() const;
    void setFairnessEnforceSongNight(bool enabled);
    bool fairnessDetectNameChange() const;
    void setFairnessDetectNameChange(bool enabled);
    // Preview audio device
    QString previewAudioDevice();
    void setPreviewAudioDevice(const QString &device);
    bool audioUseFader();
    bool audioUseFaderBm();
    void setAudioUseFader(bool fader);
    void setAudioUseFaderBm(bool fader);
    int audioVolume();
    void setAudioVolume(int volume);
    QString cdgDisplayBackgroundImage();
    void setCdgDisplayBackgroundImage(QString imageFile);
    BgMode bgMode();
    void setBgMode(BgMode mode);
    QString bgSlideShowDir();
    void setBgSlideShowDir(QString dir);
    bool audioDownmix();
    void setAudioDownmix(bool downmix);
    bool audioDownmixBm();
    void setAudioDownmixBm(bool downmix);
    bool audioDetectSilence();
    bool audioDetectSilenceBm();
    void setAudioDetectSilence(bool enabled);
    void setAudioDetectSilenceBm(bool enabled);
    QString audioOutputDevice();
    QString audioOutputDeviceBm();
    void setAudioOutputDevice(QString device);
    void setAudioOutputDeviceBm(QString device);
    int audioBackend();
    void setAudioBackend(int index);
    QString recordingContainer();
    void setRecordingContainer(QString container);
    QString recordingCodec();
    void setRecordingCodec(QString codec);
    QString recordingInput();
    void setRecordingInput(QString input);
    QString recordingOutputDir();
    void setRecordingOutputDir(QString path);
    bool recordingEnabled();
    void setRecordingEnabled(bool enabled);
    QString recordingRawExtension();
    void setRecordingRawExtension(QString ext);
    // Break music (filler between singers)
    QString breakMusicDir();
    void setBreakMusicDir(QString dir);
    bool breakMusicEnabled();
    void setBreakMusicEnabled(bool enabled);
    int cdgOffsetTop();
    int cdgOffsetBottom();
    int cdgOffsetLeft();
    int cdgOffsetRight();
    bool ignoreAposInSearch();
    int videoOffsetMs();
    bool bmShowFilenames();
    void bmSetShowFilenames(bool show);
    bool bmShowMetadata();
    void bmSetShowMetadata(bool show);
    QString youtubeApiKey() const;
    void setYoutubeApiKey(const QString &apiKey);
    int bmVolume();
    void bmSetVolume(int bmVolume);
    int bmPlaylistIndex();
    void bmSetPlaylistIndex(int index);
    int mplxMode();
    void setMplxMode(int mode);
    bool karaokeAutoAdvance();
    int karaokeAATimeout();
    void setKaraokeAATimeout(int secs);
    bool karaokeAAAlertEnabled();
    void setKaraokeAAAlertEnabled(bool enabled);
    QFont karaokeAAAlertFont();
    void setKaraokeAAAlertFont(QFont font);
    bool showQueueRemovalWarning();
    bool showSingerRemovalWarning();
    bool showSongInterruptionWarning();
    bool showSongPauseStopWarning();
    QColor alertTxtColor();
    QColor alertBgColor();
    bool bmAutoStart();
    void setBmAutoStart(bool enabled);
    int cdgDisplayOffset();
    QFont bookCreatorTitleFont();
    QFont bookCreatorArtistFont();
    QFont bookCreatorHeaderFont();
    QFont bookCreatorFooterFont();
    QString bookCreatorHeaderText();
    QString bookCreatorFooterText();
    bool bookCreatorPageNumbering();
    int bookCreatorSortCol();
    double bookCreatorMarginRt();
    double bookCreatorMarginLft();
    double bookCreatorMarginTop();
    double bookCreatorMarginBtm();
    int bookCreatorCols();
    int bookCreatorPageSize();
    bool eqKBypass();
    int getEqKLevel(int band);
    bool eqBBypass();
    int getEqBLevel(int band);
    int requestServerInterval();
    bool bmKCrossFade();
    bool requestRemoveOnRotAdd();
    bool requestDialogAutoShow();
    bool checkUpdates();
    int updatesBranch();
    [[nodiscard]] int theme() const;
    const QPoint durationPosition();
    bool dbDirectoryWatchEnabled();
    SfxEntryList getSfxEntries();
    void addSfxEntry(SfxEntry entry);
    void setSfxEntries(SfxEntryList entries);
    [[nodiscard]] int estimationSingerPad() const;
    void setEstimationSingerPad(int secs);
    [[nodiscard]] int estimationEmptySongLength() const;
    void setEstimationEmptySongLength(int secs);
    [[nodiscard]] bool estimationSkipEmptySingers() const;
    void setEstimationSkipEmptySingers(bool skip);
    [[nodiscard]] bool rotationDisplayPosition() const;
    void setRotationDisplayPosition(bool show);
    int currentRotationPosition();
    bool dbSkipValidation();
    bool dbLazyLoadDurations();
    int systemId();
    QFont cdgRemainFont();
    QColor cdgRemainTextColor();
    QColor cdgRemainBgColor();
    [[nodiscard]] bool rotationShowNextSong() const;
    void sync();
    bool previewEnabled();
    bool showMainWindowVideo();
    bool showMainWindowSoundClips();
    void setShowMplxControls(bool show);
    bool showMplxControls();
    void setShowMainWindowSoundClips(const bool &show);
    bool showMainWindowNowPlaying();
    void setShowMainWindowNowPlaying(const bool &show);
    int mainWindowVideoSize();
    void setMainWindowVideoSize(const PreviewSize &size);
    bool enforceAspectRatio();
    void setEnforceAspectRatio(const bool &enforce);
    QString auxTickerFile();
    QString uuid();
    uint slideShowInterval();
    int lastSingerAddPositionType();
    void saveShortcutKeySequence(const QString &name, const QKeySequence &sequence);
    QKeySequence loadShortcutKeySequence(const QString &name);
    bool cdgPrescalingEnabled();
    [[nodiscard]] bool rotationAltSortOrder() const;
    bool treatAllSingersAsRegs();

signals:
    void treatAllSingersAsRegsChanged(bool enabled);
    void enforceAspectRatioChanged(const bool &enforce);
    void requestServerVenueChanged(int venueId);
    void requestServerUrlChanged(const QString &url);
    void requestServerCredentialsChanged();
    void mplxModeChanged(int mode);
    void karaokeAutoAdvanceChanged(bool enabled);
    void showQueueRemovalWarningChanged(bool enabled);
    void showSingerRemovalWarningChanged(bool enabled);
    void showSongInterruptionWarningChanged(bool enabled);
    void showSongStopPauseWarningChanged(bool enabled);
    void requestServerIntervalChanged(int interval);
    void requestServerEnabledChanged(bool enabled);
    void rotationDisplayPositionChanged(bool show);
    void rotationDurationSettingsModified();
    void rotationShowNextSongChanged(bool show);
    void remainOffsetChanged(int offsetR, int offsetB);
    void previewEnabledChanged(bool enabled);
    void videoOffsetChanged(int offsetMs);
    void lastSingerAddPositionTypeChanged(int type);
    void venueConfigChanged();
    void gigSettingsChanged();
    void rotationStyleChanged(int style);
    void djListChanged();
    void activeDjChanged(const QString &dj);
    void shortcutsChanged();
    void crashReportingEnabledChanged(bool enabled);


public slots:
    void setShowMainWindowVideo(bool show);
    void setTreatAllSingersAsRegs(bool enabled);
    void setRotationAltSortOrder(bool enabled);
    void setCdgPrescalingEnabled(bool enabled);
    void setSlideShowInterval(int secs);
    void setHardwareAccelEnabled(bool enabled);
    void setDbDoubleClickAddsSong(bool enabled);
    void setDurationPosition(QPoint pos);
    void resetDurationPosition();
    void setRemainRtOffset(int offset);
    void setRemainBtmOffset(int offset);
    void dbSetLazyLoadDurations(bool val);
    void dbSetSkipValidation(bool val);
    void setBmKCrossfade(bool enabled);
    void setShowCdgWindow(bool show);
    void setCdgWindowFullscreen(bool fullScreen);
    void setCdgOffsetTop(int pixels);
    void setCdgOffsetBottom(int pixels);
    void setCdgOffsetLeft(int pixels);
    void setCdgOffsetRight(int pixels);
    void setShowQueueRemovalWarning(bool show);
    void setShowSingerRemovalWarning(bool show);
    void setKaraokeAutoAdvance(bool enabled);
    void setShowSongInterruptionWarning(bool enabled);
    void setAlertBgColor(QColor color);
    void setAlertTxtColor(QColor color);
    void setIgnoreAposInSearch(bool ignore);
    void setShowSongPauseStopWarning(bool enabled);
    void setBookCreatorArtistFont(QFont font);
    void setBookCreatorTitleFont(QFont font);
    void setBookCreatorHeaderFont(QFont font);
    void setBookCreatorFooterFont(QFont font);
    void setBookCreatorHeaderText(QString text);
    void setBookCreatorFooterText(QString text);
    void setBookCreatorPageNumbering(bool show);
    void setBookCreatorSortCol(int col);
    void setBookCreatorMarginRt(double margin);
    void setBookCreatorMarginLft(double margin);
    void setBookCreatorMarginTop(double margin);
    void setBookCreatorMarginBtm(double margin);
    void setEqKBypass(bool bypass);
    void setEqKLevel(int band, int level);
    void setEqBBypass(bool bypass);
    void setEqBLevel(int band, int level);
    void setRequestServerInterval(int interval);
    void setTickerShowRotationInfo(bool show);
    void setRequestRemoveOnRotAdd(bool remove);
    void setRequestDialogAutoShow(bool enabled);
    void setCheckUpdates(bool enabled);
    void setUpdatesBranch(int index);
    void setTheme(int theme);
    void setBookCreatorCols(int cols);
    void setBookCreatorPageSize(int size);
    void setStoreDownloadDir(QString path);
    void setLogEnabled(bool enabled);
    void setLogVisible(bool visible);
    void setLogDir(QString path);
    void setCurrentRotationPosition(int position);
    void dbSetDirectoryWatchEnabled(bool val);
    void setSystemId(int id);
    void setCdgRemainEnabled(bool enabled);
    void setCdgRemainFont(QFont font);
    void setCdgRemainTextColor(QColor color);
    void setCdgRemainBgColor(QColor color);
    void setRotationShowNextSong(bool show);
    void setProgressiveSearchEnabled(bool enabled);
    void setPreviewEnabled(bool enabled);
    void setVideoOffsetMs(int offset);
    void setLastSingerAddPositionType(int type);
    void setNoShowTimeoutSeconds(int seconds);

    // ── Tip/Payment settings ────────────────────────────────────────────
    bool tipNotificationEnabled() const;
    void setTipNotificationEnabled(bool enabled);
    bool tipSoundEnabled() const;
    void setTipSoundEnabled(bool enabled);
    bool tipShoutoutEnabled() const;
    void setTipShoutoutEnabled(bool enabled);
    int tipTotalCents() const;
    void setTipTotalCents(int cents);


    void setKjBufferSeconds(int seconds);
    void setLineJumpEnabled(bool enabled);
    void setLimitOneSongPerSinger(bool enabled);
    void setBlockRepeatSongs(bool enabled);
    void setVenueGeofenceRadius(double miles);
    void setVenueGeofenceEnabled(bool enabled);
    void setVenueLon(double lon);
    void setVenueLat(double lat);
    void setVenueAddress(const QString &address);
    // KJ PIN removed — singers use nearby/QR/slug to find venues

signals:
    void tipNotificationEnabledChanged(bool enabled);
    void tipSoundEnabledChanged(bool enabled);
    void tipShoutoutEnabledChanged(bool enabled);

private:
    // Internal helper for DJ-specific settings
    QString djKey(const QString &baseKey) const;
};

#endif // SETTINGS_H
