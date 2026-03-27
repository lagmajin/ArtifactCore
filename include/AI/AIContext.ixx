module;
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <vector>
#include <memory>

export module Core.AI.Context;

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
    void addSelectedLayer(const QString& layerData) { selectedLayers_.push_back(layerData); }
    void setUserPrompt(const QString& prompt) { userPrompt_ = prompt; }
    void setSystemPrompt(const QString& prompt) { systemPrompt_ = prompt; }
    
    // ── Getter メソッド（AI 分析用）─
    QString projectSummary() const { return projectSummary_; }
    QString activeCompositionId() const { return activeCompositionId_; }
    std::vector<QString> selectedLayers() const { return selectedLayers_; }
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

        QJsonArray layers;
        for (const auto& l : selectedLayers_) {
            layers.append(l);
        }
        root["selectedLayers"] = layers;

        QJsonArray actions;
        for (const auto& a : recentActions_) {
            QJsonObject act;
            act["type"] = static_cast<int>(a.type);
            act["targetId"] = a.targetId;
            act["details"] = a.details;
            actions.append(act);
        }
        root["recentActions"] = actions;

        return root;
    }

    QString toJsonString() const {
        QJsonDocument doc(toJson());
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

private:
    QString projectSummary_;
    QString activeCompositionId_;
    std::vector<QString> selectedLayers_;
    std::vector<UserAction> recentActions_;
    QString userPrompt_;
    QString systemPrompt_;
};

} // namespace ArtifactCore
