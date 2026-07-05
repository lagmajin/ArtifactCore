module;
#include <utility>
#include <QString>
#include <QStringView>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

export module Core.AI.ToolExecutor;

import Core.AI.Describable;

export namespace ArtifactCore {

/**
 * @brief Logic for executing tool calls requested by the AI
 */
class AIToolExecutor {
public:
    static AIToolExecutor& instance() {
        static AIToolExecutor executor;
        return executor;
    }

    /**
     * @brief Execute a tool call formatted as JSON
     * 
     * Expected format:
     * {
     *   "class": "ObjectDetector",
     *   "method": "detect",
     *   "arguments": [ ... ]
     * }
     */
    QVariant execute(const QJsonObject& toolCall) {
        const QString className = toolCall["class"].toString();
        const QString methodName = toolCall["method"].toString();
        QJsonArray argsArray = toolCall["arguments"].toArray();

        // 1. Find the object in registry
        const IDescribable* constObj = DescriptionRegistry::instance().getDescribable(QStringView{className});
        if (!constObj) return QVariant();

        // Note: For now we const_cast because the registry currently only returns const.
        IDescribable* obj = const_cast<IDescribable*>(constObj);
        
        // 2. Invoke
        QVariantList args = jsonArgsToVariantList(argsArray);
        return obj->invokeMethod(QStringView{methodName}, args);
    }

    /**
     * @brief Helper to convert QJsonArray to QVariantList
     */
    static QVariantList jsonArgsToVariantList(const QJsonArray& args) {
        QVariantList list;
        for (const auto& v : args) {
            list.append(v.toVariant());
        }
        return list;
    }
};

} // namespace ArtifactCore
