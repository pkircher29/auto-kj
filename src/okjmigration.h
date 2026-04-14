#ifndef OKJMIGRATION_H
#define OKJMIGRATION_H

#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

class MigrationManager {
public:
    static void migrateIfNeeded() {
        QString oldOrg = "AutoKJ";
        QString oldApp = "AutoKJ";
        QString newOrg = "Auto-KJ";
        QString newApp = "Auto-KJ";

        // Determine new path
        QCoreApplication::setOrganizationName(newOrg);
        QCoreApplication::setApplicationName(newApp);
        QString newPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        
        QDir newDir(newPath);
        if (newDir.exists() && newDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).count() > 0) {
            // New directory already has data, skip migration
            return;
        }

        // Determine old path
        QCoreApplication::setOrganizationName(oldOrg);
        QCoreApplication::setApplicationName(oldApp);
        QString oldPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        
        // Reset to new names
        QCoreApplication::setOrganizationName(newOrg);
        QCoreApplication::setApplicationName(newApp);

        QDir oldDir(oldPath);
        if (!oldDir.exists()) {
            return;
        }

        qInfo() << "Migrating data from " << oldPath << " to " << newPath;

        if (!newDir.exists()) {
            newDir.mkpath(".");
        }

        // Copy settings
        QString oldIni = oldPath + QDir::separator() + "autokj.ini";
        QString newIni = newPath + QDir::separator() + "auto-kj.ini";
        if (QFile::exists(oldIni)) {
            QFile::copy(oldIni, newIni);
        }

        // Copy database
        QString oldDb = oldPath + QDir::separator() + "autokj.sqlite";
        QString newDb = newPath + QDir::separator() + "auto-kj.sqlite";
        if (QFile::exists(oldDb)) {
            QFile::copy(oldDb, newDb);
        }

        // Copy backups directory if exists
        QDir oldBackups(oldPath + QDir::separator() + "backups");
        if (oldBackups.exists()) {
            copyRecursively(oldBackups.absolutePath(), newPath + QDir::separator() + "backups");
        }
    }

private:
    static bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath) {
        QDir srcDir(srcFilePath);
        if (!srcDir.exists()) return false;
        QDir tgtDir(tgtFilePath);
        if (!tgtDir.exists()) tgtDir.mkpath(".");

        foreach (const QString &file, srcDir.entryList(QDir::Files)) {
            QFile::copy(srcFilePath + QDir::separator() + file, tgtFilePath + QDir::separator() + file);
        }
        foreach (const QString &dir, srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            copyRecursively(srcFilePath + QDir::separator() + dir, tgtFilePath + QDir::separator() + dir);
        }
        return true;
    }
};

#endif // OKJMIGRATION_H

