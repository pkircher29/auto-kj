#ifndef SINGERIDENTITYMANAGER_H
#define SINGERIDENTITYMANAGER_H

#include <QObject>
#include <QString>
#include "models/tablemodelrotation.h"

class SingerIdentityManager : public QObject {
    Q_OBJECT
public:
    struct MatchResult {
        bool isDuplicate;
        QString matchedName;
        int singerId;
        float confidence;     // 0.0 - 1.0
        QString explanation;
    };

    explicit SingerIdentityManager(TableModelRotation& rotModel, QObject* parent = nullptr);

    MatchResult checkForDuplicate(const QString& newName);
    
    // Merge: rename singer in rotation, record alias
    void mergeSingers(int existingId, const QString& formerName, const QString& newName);
    
    // Permanently link an alias for this venue
    void recordAlias(const QString& canonical, const QString& alias);
    
    // Get canonical name for a known alias
    QString resolveAlias(const QString& name);

private:
    TableModelRotation& m_rotModel;
    int levenshteinDistance(const QString& s1, const QString& s2);
};

#endif // SINGERIDENTITYMANAGER_H
