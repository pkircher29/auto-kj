#include "autobackupmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStringList>
#include <algorithm>
#include <spdlog/spdlog.h>

AutoBackupManager::AutoBackupManager(const QString &dbPath, QObject *parent)
    : QObject(parent), m_dbPath(dbPath) {}

QString AutoBackupManager::backupDir() const {
    // Use a "backups" subdirectory alongside the DB by default
    QFileInfo fi(m_dbPath);
    return fi.absoluteDir().absolutePath() + QDir::separator() + "backups";
}

bool AutoBackupManager::runBackup() {
    if (m_dbPath.isEmpty()) {
        spdlog::get("logger")->warn("[AutoBackup] No DB path configured — skipping backup");
        return false;
    }

    QString dir = backupDir();
    QDir().mkpath(dir);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmm");
    QString destName = dir + QDir::separator() + "auto-kj_" + timestamp + ".sqlite";

    if (!QFile::copy(m_dbPath, destName)) {
        spdlog::get("logger")->error("[AutoBackup] Failed to copy DB to {}", destName.toStdString());
        return false;
    }

    spdlog::get("logger")->info("[AutoBackup] Backup written to {}", destName.toStdString());
    pruneOldBackups(dir, 30);
    return true;
}

void AutoBackupManager::pruneOldBackups(const QString &dir, int keepCount) {
    QDir d(dir);
    QStringList filters{"auto-kj_*.sqlite"};
    QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

    // files are sorted oldest-first (Reversed); keep the newest keepCount
    while (files.size() > keepCount) {
        QFile::remove(files.first().absoluteFilePath());
        spdlog::get("logger")->info("[AutoBackup] Pruned old backup: {}",
                                    files.first().fileName().toStdString());
        files.removeFirst();
    }
}
