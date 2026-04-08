module;
#include <utility>
#include <memory>
#include <functional>
#include <QString>
#include <QMap>
#include <QStringList>

export module Core.AI.LocalAgent;

import Core.AI.Context;

export namespace ArtifactCore {

/**
 * @brief ローカル AI による分析結果
 */
struct LocalAnalysisResult {
    QString intent;                    // 質問の意図 ("visibility", "animation", "color", "audio", "render", "unknown")
    QMap<QString, QString> entities;   // 抽出されたエンティティ（レイヤー名、プロパティ名など）
    bool requiresCloud = false;        // クラウドが必要か
    QString summarizedContext;         // 要約されたコンテキスト（クラウド送信用）
    QStringList requiredData;          // 必要なデータ一覧
    QString localAnswer;               // ローカルで答えられる場合の回答
    float confidence = 1.0f;           // 確信度 (0.0-1.0)
};

/**
 * @brief ローカルで動作する軽量 AI（Tier 1）のインターフェース。
 * ONNX Runtime や小規模なローカル LLM をバックエンドに持つことを想定。
 */
class LocalAIAgent {
public:
    virtual ~LocalAIAgent() = default;

    /**
     * @brief AI モデルの初期化（モデルのロードなど）
     */
    virtual bool initialize(const QString& modelPath) = 0;

    /**
     * @brief 現在のコンテキストを分析し、ユーザーの意図を推論する
     * @param context 収集されたアプリケーションのコンテキスト
     * @return 推論結果（意図のサマリーや、クラウドへのエスカレーションが必要かどうかのフラグ）
     */
    virtual QString analyzeContext(const AIContext& context) = 0;

    /**
     * @brief 特定のプロパティ値の自動調整など、軽量なタスクを即座に実行する
     * @return 成功した場合は JSON 形式のパラメータなどを返す
     */
    virtual QString predictParameter(const QString& targetProperty, const AIContext& context) = 0;

    /**
     * @brief クラウド AI（Tier 2）の助けが必要かどうかを判断する
     */
    virtual bool requiresCloudEscalation(const QString& userPrompt, const AIContext& context) = 0;
    
    /**
     * @brief ユーザーの質問を分析し、必要な情報収集とクラウド要不要を判断する
     * @param question ユーザーの質問
     * @param context 現在のアプリケーションコンテキスト
     * @return 分析結果（意図、必要データ、クラウド要不要、ローカル回答など）
     */
    virtual LocalAnalysisResult analyzeUserQuestion(const QString& question, const AIContext& context) = 0;

    /**
     * @brief ローカル LLM で会話応答を生成する
     * @param systemPrompt システムプロンプト
     * @param userPrompt ユーザープロンプト
     * @param context 現在のアプリケーションコンテキスト
     * @return 生成された応答テキスト
     */
    virtual QString generateChatResponse(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context) = 0;

    /**
     * @brief ローカル LLM でストリーミング会話応答を生成する
     * @param systemPrompt システムプロンプト
     * @param userPrompt ユーザープロンプト
     * @param context 現在のアプリケーションコンテキスト
     * @param tokenCallback トークンごとに呼ばれるコールバック。false を返すと生成を中断する。
     * @return 生成された応答テキスト全体
     */
    virtual QString generateChatResponseStreaming(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const std::function<bool(const QString& piece)>& tokenCallback) {
        return generateChatResponse(systemPrompt, userPrompt, context);
    }
    
    /**
     * @brief 機密情報をフィルタリングする
     * @param text フィルタリング対象のテキスト
     * @return 匿名化されたテキスト
     */
    virtual QString filterSensitiveInfo(const QString& text) = 0;

    /**
     * @brief 最後のエラーメッセージ。未実装なら空文字。
     */
    virtual QString lastError() const { return {}; }
};

using LocalAIAgentPtr = std::shared_ptr<LocalAIAgent>;

} // namespace ArtifactCore
