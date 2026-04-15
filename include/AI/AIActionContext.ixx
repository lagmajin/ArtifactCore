module;
#include <utility>
#include <QString>
#include <QStringView>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include <QJsonObject>
export module Core.AI.ActionContext;


export namespace ArtifactCore {

/**
 * @brief Dynamic context of the application for AI
 * 
 * Provides real-time state like current time, selected layers,
 * and other dynamic properties to the AI prompt generator.
 */
class AIActionContext {
public:
    static AIActionContext& instance() {
        static AIActionContext ctx;
        return ctx;
    }

    /**
     * @brief Set a dynamic context value
     */
    void set(const QString& key, const QVariant& value) {
        contextData_[key] = value;
    }

    /**
     * @brief Get a dynamic context value
     */
    QVariant get(const QString& key) const {
        return contextData_.value(key);
    }

    /**
     * @brief Clear all dynamic context
     */
    void clear() {
        contextData_.clear();
    }

    /**
     * @brief Generate a text description of the current context for AI prompt
     */
    QString describeContext() const {
        QString result = "## Current Application State\n\n";
        result += QStringLiteral("- **Current Time**: ") + QDateTime::currentDateTime().toString(Qt::ISODate) +
                  QStringLiteral("\n");
        
        if (contextData_.isEmpty()) {
            result += "- No specific context active.\n";
            return result;
        }

        for (auto it = contextData_.constBegin(); it != contextData_.constEnd(); ++it) {
            result += QStringLiteral("- **") + it.key() + QStringLiteral("**: ") + it.value().toString() +
                      QStringLiteral("\n");
        }
        
        return result + "\n";
    }

    /**
     * @brief Convert context to JSON
     */
    QJsonObject toJson() const {
        QJsonObject json;
        json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        for (auto it = contextData_.constBegin(); it != contextData_.constEnd(); ++it) {
            json[it.key()] = QJsonValue::fromVariant(it.value());
        }
        return json;
    }

private:
    AIActionContext() = default;
    QMap<QString, QVariant> contextData_;
};

} // namespace ArtifactCore
