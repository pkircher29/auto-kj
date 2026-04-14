#ifndef AUTOBACKUPMANAGER_H
#define AUTOBACKUPMANAGER_H

#include <QObject>
#include <QString>
#include "settings.h"

class AutoBackupManager : public QObject {
    Q_OBJECT
public:
    explicit AutoBackupManager(const QString &dbPath, QObject *parent = nullptr);

    // Run immediately — copies DB to timestamped backup, prunes old backups
    // Returns true on success
    bool runBackup();

    // Path where backups are stored (from settings, or same dir as DB)
    QString backupDir() const;

private:
    QString m_dbPath;
    Settings m_settings;

    void pruneOldBackups(const QString &dir, int keepCount);
};

#endif // AUTOBACKUPMANAGER_H
