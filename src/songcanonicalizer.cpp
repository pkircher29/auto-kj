#include "songcanonicalizer.h"

#include <QRegularExpression>

namespace {
QString normalizePart(QString value)
{
    value = value.normalized(QString::NormalizationForm_KD).toLower();
    value.replace('&', " and ");
    // Drop bracketed tags often used for mix/version labels.
    value.remove(QRegularExpression("\\[[^\\]]*\\]"));
    value.remove(QRegularExpression("\\([^\\)]*\\)"));
    // Remove common non-identity qualifiers so vendor/version variants collapse together.
    value.remove(QRegularExpression(
        "\\b(karaoke|karaoke version|version|ver|instrumental|radio edit|clean|explicit|"
        "in the style of|as made famous by|with vocals|w vocals)\\b"));
    value.replace(QRegularExpression("[^a-z0-9]+"), " ");
    return value.simplified();
}
}

namespace okj {
QString canonicalSongKey(const QString &artist, const QString &title)
{
    const QString artistKey = normalizePart(artist);
    const QString titleKey = normalizePart(title);
    return artistKey + "|" + titleKey;
}
}
