#include "singeridentitymanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>
#include <spdlog/spdlog.h>
#include <algorithm>

SingerIdentityManager::SingerIdentityManager(TableModelRotation& rotModel, QObject* parent)
    : QObject(parent), m_rotModel(rotModel) {}

int SingerIdentityManager::levenshteinDistance(const QString& s1, const QString& s2) {
    const int len1 = s1.size();
    const int len2 = s2.size();
    std::vector<int> col(len2 + 1);
    std::vector<int> prevCol(len2 + 1);
    
    for (int i = 0; i < prevCol.size(); i++) prevCol[i] = i;
    
    for (int i = 0; i < len1; i++) {
        col[0] = i + 1;
        for (int j = 0; j < len2; j++) {
            int cost = (s1[i].toLower() == s2[j].toLower()) ? 0 : 1;
            col[j + 1] = std::min({prevCol[1 + j] + 1, col[j] + 1, prevCol[j] + cost});
        }
        col.swap(prevCol);
    }
    return prevCol[len2];
}

SingerIdentityManager::MatchResult SingerIdentityManager::checkForDuplicate(const QString& newName) {
    MatchResult result{false, "", -1, 0.0f, ""};
    QString normalizedSearch = newName.toLower().trimmed();
    
    // Check known aliases first
    QString canonical = resolveAlias(normalizedSearch);
    if (!canonical.isEmpty() && canonical.toLower() != normalizedSearch) {
        // If the canonical name is currently in the rotation, return it
        auto singers = m_rotModel.singers();
        for (const auto& sName : singers) {
            if (sName.toLower() == canonical.toLower()) {
                const auto& singer = m_rotModel.getSingerByName(sName);
                return {true, sName, singer.id, 1.0f, "Known alias of " + sName};
            }
        }
    }

    // Check against active rotation
    auto singers = m_rotModel.singers();
    for (const auto& sName : singers) {
        QString normalizedRot = sName.toLower().trimmed();
        if (normalizedRot == normalizedSearch) {
            const auto& singer = m_rotModel.getSingerByName(sName);
            return {true, sName, singer.id, 1.0f, "Exact match"};
        }

        int dist = levenshteinDistance(normalizedSearch, normalizedRot);
        if (dist <= 2) {
            float conf = 0.85f - (dist * 0.05f);
            if (conf >= 0.70f && conf > result.confidence) {
                const auto& singer = m_rotModel.getSingerByName(sName);
                result = {true, sName, singer.id, conf, "Similar matching name"};
            }
        } else if (normalizedSearch.contains(normalizedRot) || normalizedRot.contains(normalizedSearch)) {
            // Substring match
            if (result.confidence < 0.65f) {
                const auto& singer = m_rotModel.getSingerByName(sName);
                result = {true, sName, singer.id, 0.65f, "Substring match"};
            }
        }
    }

    if (result.confidence < 0.70f) {
        result.isDuplicate = false;
    }

    return result;
}

void SingerIdentityManager::mergeSingers(int existingId, const QString& formerName, const QString& newName) {
    // The existing singer is already in rotation. We rename them if the new request name is different,
    // or maybe the user prefers the existing rotation name. Assuming newName is the canonical one now.
    // Actually, usually the one already in the list might be canonical or former. 
    // We'll rename the row to `newName (formerly oldName)` or just `newName`.
    // Wait, the plan says: renamed to canonical name + "(formerly Robbie)" notation
    
    QString display = newName + " (formerly " + formerName + ")";
    m_rotModel.singerSetName(existingId, display);
    
    // Save the alias link
    recordAlias(newName, formerName);
}

void SingerIdentityManager::recordAlias(const QString& canonical, const QString& alias) {
    if (canonical.toLower() == alias.toLower()) return;
    
    QSqlQuery query;
    query.prepare("INSERT INTO singerAliases (canonical_name, alias_name, first_seen, last_seen) "
                  "VALUES (:canonical, :alias, :now, :now) "
                  "ON CONFLICT(alias_name) DO UPDATE SET canonical_name=:canonical, last_seen=:now");
    query.bindValue(":canonical", canonical);
    query.bindValue(":alias", alias);
    query.bindValue(":now", QDateTime::currentDateTime());
    if (!query.exec()) {
        spdlog::get("logger")->error("Failed to record alias: {}", query.lastError().text().toStdString());
    }
}

QString SingerIdentityManager::resolveAlias(const QString& name) {
    QSqlQuery query;
    query.prepare("SELECT canonical_name FROM singerAliases WHERE alias_name = :alias COLLATE NOCASE");
    query.bindValue(":alias", name);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}
