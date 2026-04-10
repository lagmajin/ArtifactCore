module;
#include <QString>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

export module Core.AI.Context;
import std;

export namespace ArtifactCore {

/**
 * @brief AIに渡すためのアプリケーションの文脈（コンテキスト）を表現するクラス。
 * ローカルAIがこのデータを収集し、必要に応じてクラウドAIへ送るペイロードの基礎となります。
 */
class AIContext {
public:
    enum class ActionType {
        SelectLayer,
        ModifyProperty,
        AddEffect,
        PlayTimeline,
        Unknown
    };

    struct UserAction {
        ActionType type;
        QString targetId;
        QString details;
        qint64 timestampMs;
    };

    AIContext() = default;
    ~AIContext() = default;

    // ── 状態の記録 ──
    void setProjectSummary(const QString& summary) { projectSummary_ = summary; }
    void setActiveCompositionId(const QString& compId) { activeCompositionId_ = compId; }
    void setActiveCompositionName(const QString& name) { activeCompositionName_ = name; }
    void addSelectedLayer(const QString& layerData) { selectedLayers_.push_back(layerData); }
    void clearCompositionNames() { compositionNames_.clear(); }
    void addCompositionName(const QString& name) { compositionNames_.push_back(name); }
    void setCompositionCount(int count) { compositionCount_ = count; }
    void setTotalLayerCount(int count) { totalLayerCount_ = count; }
    void setTotalEffectCount(int count) { totalEffectCount_ = count; }
    void setHeavyCompositionCount(int count) { heavyCompositionCount_ = count; }
    void clearHeavyCompositionNames() { heavyCompositionNames_.clear(); }
    void addHeavyCompositionName(const QString& name) { heavyCompositionNames_.push_back(name); }
    void setUserPrompt(const QString& prompt) { userPrompt_ = prompt; }
    void setSystemPrompt(const QString& prompt) { systemPrompt_ = prompt; }
    
    // ── Getter メソッド（AI 分析用）─
    QString projectSummary() const { return projectSummary_; }
    QString activeCompositionId() const { return activeCompositionId_; }
    QString activeCompositionName() const { return activeCompositionName_; }
    std::vector<QString> selectedLayers() const { return selectedLayers_; }
    QStringList compositionNames() const { return compositionNames_; }
    int compositionCount() const { return compositionCount_; }
    int totalLayerCount() const { return totalLayerCount_; }
    int totalEffectCount() const { return totalEffectCount_; }
    int heavyCompositionCount() const { return heavyCompositionCount_; }
    QStringList heavyCompositionNames() const { return heavyCompositionNames_; }
    std::vector<UserAction> recentActions() const { return recentActions_; }
    QString userPrompt() const { return userPrompt_; }
    QString systemPrompt() const { return systemPrompt_; }

    // 直近のユーザーアクションを記録（最大保持数を決めてリングバッファ的に扱うと良い）
    void logUserAction(ActionType type, const QString& targetId, const QString& details) {
        recentActions_.push_back({type, targetId, details, QDateTime::currentMSecsSinceEpoch()});
        if (recentActions_.size() > 50) {
            recentActions_.erase(recentActions_.begin());
        }
    }

    // ── ペイロード生成 ──
    // ローカルLLMやクラウドLLMのプロンプトとして使いやすいJSON形式に変換
    QJsonObject toJson() const {
        QJsonObject root;
        root["projectSummary"] = projectSummary_;
        root["activeComposition"] = activeCompositionId_;
        root["activeCompositionName"] = activeCompositionName_;
        QJsonArray compositionNameArray;
        for (const auto& name : compositionNames_) {
            compositionNameArray.append(name);
        }
        root["compositionNames"] = compositionNameArray;
        root["compositionCount"] = compositionCount_;
        root["totalLayerCount"] = totalLayerCount_;
        root["totalEffectCount"] = totalEffectCount_;
        root["heavyCompositionCount"] = heavyCompositionCount_;

        QJsonArray layers;
        for (const auto& l : selectedLayers_) {
            layers.append(l);
        }
        root["selectedLayers"] = layers;

        QJsonArray heavyNames;
        for (const auto& name : heavyCompositionNames_) {
            heavyNames.append(name);
        }
        root["heavyCompositionNames"] = heavyNames;

        QJsonArray actions;
        for (const auto& a : recentActions_) {
            QJsonObject act;
            act["type"] = static_cast<int>(a.type);
            act["targetId"] = a.targetId;
            act["details"] = a.details;
            act["timestampMs"] = static_cast<qint64>(a.timestampMs);
            actions.append(act);
        }
        root["recentActions"] = actions;

        return root;
    }

    QString toJsonString() const {
        QJsonDocument doc(toJson());
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    static AIContext fromJson(const QJsonObject& root)
    {
        AIContext context;
        context.projectSummary_ = root.value(QStringLiteral("projectSummary")).toString();
        context.activeCompositionId_ = root.value(QStringLiteral("activeComposition")).toString();
        context.activeCompositionName_ = root.value(QStringLiteral("activeCompositionName")).toString();

        const QJsonArray compositionNameArray = root.value(QStringLiteral("compositionNames")).toArray();
        for (const auto& value : compositionNameArray) {
            context.compositionNames_.push_back(value.toString());
        }

        context.compositionCount_ = root.value(QStringLiteral("compositionCount")).toInt();
        context.totalLayerCount_ = root.value(QStringLiteral("totalLayerCount")).toInt();
        context.totalEffectCount_ = root.value(QStringLiteral("totalEffectCount")).toInt();
        context.heavyCompositionCount_ = root.value(QStringLiteral("heavyCompositionCount")).toInt();

        const QJsonArray selectedLayerArray = root.value(QStringLiteral("selectedLayers")).toArray();
        for (const auto& value : selectedLayerArray) {
            context.selectedLayers_.push_back(value.toString());
        }

        const QJsonArray heavyNamesArray = root.value(QStringLiteral("heavyCompositionNames")).toArray();
        for (const auto& value : heavyNamesArray) {
            context.heavyCompositionNames_.push_back(value.toString());
        }

        const QJsonArray actionsArray = root.value(QStringLiteral("recentActions")).toArray();
        for (const auto& value : actionsArray) {
            const QJsonObject act = value.toObject();
            UserAction action;
            action.type = static_cast<ActionType>(act.value(QStringLiteral("type")).toInt(static_cast<int>(ActionType::Unknown)));
            action.targetId = act.value(QStringLiteral("targetId")).toString();
            action.details = act.value(QStringLiteral("details")).toString();
            action.timestampMs = act.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
            if (action.timestampMs == 0) {
                action.timestampMs = QDateTime::currentMSecsSinceEpoch();
            }
            context.recentActions_.push_back(action);
        }

        return context;
    }

private:
    QString projectSummary_;
    QString activeCompositionId_;
    QString activeCompositionName_;
    std::vector<QString> selectedLayers_;
    QStringList compositionNames_;
    int compositionCount_ = 0;
    int totalLayerCount_ = 0;
    int totalEffectCount_ = 0;
    int heavyCompositionCount_ = 0;
    QStringList heavyCompositionNames_;
    std::vector<UserAction> recentActions_;
    QString userPrompt_;
    QString systemPrompt_;
};

} // namespace ArtifactCore
