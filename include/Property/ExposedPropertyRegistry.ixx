module;

#include <utility>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>

export module Property.ExposedPropertyRegistry;

namespace ArtifactCore {

export struct ExposedPropertyBinding {
    QString id;
    QString label;
    QString targetLayerId;
    QString internalPath;
    QVariant defaultValue;

    bool isValid() const {
        return !id.trimmed().isEmpty() &&
               !internalPath.trimmed().isEmpty();
    }

    QJsonObject toJson() const {
        return QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("targetLayerId"), targetLayerId},
            {QStringLiteral("internalPath"), internalPath},
            {QStringLiteral("defaultValue"),
             QJsonValue::fromVariant(defaultValue)},
        };
    }
};

export class ExposedPropertyRegistry {
public:
    bool add(ExposedPropertyBinding binding) {
        if (!binding.isValid()) return false;
        normalize(binding);
        const QString key = binding.id;
        if (bindings_.contains(key)) return false;
        bindings_.insert(key, std::move(binding));
        return true;
    }

    bool replace(ExposedPropertyBinding binding) {
        if (!binding.isValid()) return false;
        normalize(binding);
        bindings_[binding.id] = std::move(binding);
        return true;
    }

    bool remove(const QString& id) {
        return bindings_.remove(id.trimmed()) > 0;
    }

    bool contains(const QString& id) const {
        return bindings_.contains(id.trimmed());
    }

    ExposedPropertyBinding binding(const QString& id) const {
        return bindings_.value(id.trimmed());
    }

    QVector<ExposedPropertyBinding> bindings() const {
        QVector<ExposedPropertyBinding> result;
        result.reserve(bindings_.size());
        for (const auto& binding : bindings_) result.push_back(binding);
        return result;
    }

    QJsonArray toJson() const {
        QJsonArray result;
        for (const auto& binding : bindings_) result.append(binding.toJson());
        return result;
    }

    void fromJson(const QJsonArray& array) {
        bindings_.clear();
        for (const auto& value : array) {
            if (!value.isObject()) continue;
            const auto object = value.toObject();
            ExposedPropertyBinding binding;
            binding.id = object.value(QStringLiteral("id")).toString();
            binding.label = object.value(QStringLiteral("label")).toString();
            binding.targetLayerId =
                object.value(QStringLiteral("targetLayerId")).toString();
            binding.internalPath =
                object.value(QStringLiteral("internalPath")).toString();
            binding.defaultValue =
                object.value(QStringLiteral("defaultValue")).toVariant();
            add(std::move(binding));
        }
    }

    void clear() { bindings_.clear(); }

private:
    static void normalize(ExposedPropertyBinding& binding) {
        binding.id = binding.id.trimmed();
        binding.label = binding.label.trimmed();
        binding.targetLayerId = binding.targetLayerId.trimmed();
        binding.internalPath = binding.internalPath.trimmed();
    }

    QMap<QString, ExposedPropertyBinding> bindings_;
};

} // namespace ArtifactCore
