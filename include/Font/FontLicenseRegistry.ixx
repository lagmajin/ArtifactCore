module;

#include <utility>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QVector>

export module Font.LicenseRegistry;

namespace ArtifactCore {

export struct FontLicenseEntry {
    QString family;
    QString style;
    QString licenseFilePath;
    QString attestation;
    QString note;

    bool isAttested() const {
        return !licenseFilePath.trimmed().isEmpty() ||
               !attestation.trimmed().isEmpty();
    }

    QJsonObject toJson() const {
        return QJsonObject{
            {QStringLiteral("family"), family},
            {QStringLiteral("style"), style},
            {QStringLiteral("licenseFilePath"), licenseFilePath},
            {QStringLiteral("attestation"), attestation},
            {QStringLiteral("note"), note},
        };
    }
};

export class FontLicenseRegistry {
public:
    void set(FontLicenseEntry entry) {
        entry.family = entry.family.trimmed();
        entry.style = entry.style.trimmed();
        const QString key = makeKey(entry.family, entry.style);
        if (key.isEmpty()) return;
        entries_[key] = std::move(entry);
    }

    void remove(const QString& family, const QString& style = {}) {
        entries_.remove(makeKey(family, style));
    }

    bool contains(const QString& family, const QString& style = {}) const {
        return findEntry(family, style) != entries_.cend();
    }

    FontLicenseEntry entry(const QString& family,
                           const QString& style = {}) const {
        const auto found = findEntry(family, style);
        return found == entries_.cend() ? FontLicenseEntry{} : found.value();
    }

    bool isAttested(const QString& family, const QString& style = {}) const {
        const auto found = findEntry(family, style);
        return found != entries_.cend() && found->isAttested();
    }

    QJsonObject toJson() const {
        QJsonObject result;
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            result.insert(it.key(), it.value().toJson());
        }
        return result;
    }

    void fromJson(const QJsonObject& object) {
        entries_.clear();
        for (auto it = object.cbegin(); it != object.cend(); ++it) {
            if (!it.value().isObject()) continue;
            const QJsonObject value = it.value().toObject();
            FontLicenseEntry entry;
            entry.family = value.value(QStringLiteral("family")).toString();
            entry.style = value.value(QStringLiteral("style")).toString();
            entry.licenseFilePath =
                value.value(QStringLiteral("licenseFilePath")).toString();
            entry.attestation =
                value.value(QStringLiteral("attestation")).toString();
            entry.note = value.value(QStringLiteral("note")).toString();
            set(std::move(entry));
        }
    }

    void clear() { entries_.clear(); }

    QVector<FontLicenseEntry> entries() const {
        QVector<FontLicenseEntry> result;
        result.reserve(entries_.size());
        for (const auto& entry : entries_) result.push_back(entry);
        return result;
    }

private:
    QMap<QString, FontLicenseEntry>::const_iterator findEntry(
        const QString& family, const QString& style) const {
        const auto exact = entries_.constFind(makeKey(family, style));
        if (exact != entries_.cend()) return exact;
        if (!style.trimmed().isEmpty()) {
            return entries_.constFind(makeKey(family, {}));
        }
        return entries_.cend();
    }

    static QString makeKey(const QString& family, const QString& style) {
        const QString normalizedFamily = family.trimmed();
        if (normalizedFamily.isEmpty()) return {};
        return normalizedFamily + QStringLiteral("|") + style.trimmed();
    }

    QMap<QString, FontLicenseEntry> entries_;
};

} // namespace ArtifactCore
