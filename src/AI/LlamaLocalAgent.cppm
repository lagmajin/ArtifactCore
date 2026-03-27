module;
#include <QString>
#include <QDebug>
#include <QRegularExpression>
#include <vector>
#include <string>
#include <memory>

module Core.AI.LlamaAgent;

import std;
import Core.AI.Context;

namespace ArtifactCore {

class LlamaLocalAgent::Impl {
public:
    bool initialized = false;
    int n_predict = 128;
    float temp = 0.7f;

    Impl() {
        // llama.cpp は使用しない（ビルド問題のため）
        // ルールベースの応答生成のみ実装
    }

    ~Impl() = default;
    
    QString generateResponse(const QString& intent, const QString& collectedData) {
        Q_UNUSED(collectedData);
        
        if (intent == "visibility") {
            return "レイヤーの表示設定を確認してください。\n"
                   "1. タイムラインパネルで眼球アイコンが ON になっているか\n"
                   "2. 不透明度が 0% になっていないか\n"
                   "3. ソロ設定が有効になっていないか\n"
                   "4. レイヤーがロックされていないか\n"
                   "5. IN/OUT ポイントの範囲内にプレイヘッドがあるか";
        } else if (intent == "animation") {
            return "アニメーション設定を確認してください。\n"
                   "1. キーフレームが設定されているか\n"
                   "2. 現在のフレームがキーフレームの範囲内か\n"
                   "3. プロパティがリンク解除されていないか";
        } else if (intent == "color") {
            return "カラー設定を確認してください。\n"
                   "1. コンポジションのカラー空間設定\n"
                   "2. レイヤーにエフェクトが適用されていないか\n"
                   "3. 32bit/float カラーが有効か";
        } else if (intent == "audio") {
            return "オーディオ設定を確認してください。\n"
                   "1. ミュート設定になっていないか\n"
                   "2. オーディオレベルが 0 になっていないか\n"
                   "3. オーディオデバイスが正しく設定されているか";
        } else if (intent == "render") {
            return "レンダー設定を確認してください。\n"
                   "1. レンダーキューにジョブが追加されているか\n"
                   "2. 出力先フォルダが存在するか\n"
                   "3. 十分なディスク容量があるか";
        } else if (intent == "effect") {
            return "エフェクト設定を確認してください。\n"
                   "1. エフェクトが有効になっているか\n"
                   "2. パラメータが適切に設定されているか\n"
                   "3. エフェクトの順序が適切か";
        }
        
        return "申し訳ありません。もう少し詳しく教えていただけますか？\n"
               "例：「レイヤーが見えない」「アニメーションを追加したい」など";
    }
};

LlamaLocalAgent::LlamaLocalAgent() : impl_(std::make_unique<Impl>()) {}
LlamaLocalAgent::~LlamaLocalAgent() = default;

bool LlamaLocalAgent::initialize(const QString& modelPath) {
    Q_UNUSED(modelPath);
    // llama.cpp を使用しないため、常に true を返す
    impl_->initialized = true;
    qDebug() << "[LlamaLocalAgent] Initialized (rule-based fallback mode)";
    return true;
}

QString LlamaLocalAgent::analyzeContext(const AIContext& context) {
    // 簡易的なコンテキスト分析
    auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        return QString("選択中のレイヤー：%1").arg(selectedLayers.front());
    }
    QString activeCompId = context.activeCompositionId();
    if (!activeCompId.isEmpty()) {
        return QString("アクティブコンポジション：%1").arg(activeCompId);
    }
    return "コンテキスト情報なし";
}

QString LlamaLocalAgent::predictParameter(const QString& targetProperty, const AIContext& context) {
    Q_UNUSED(targetProperty);
    Q_UNUSED(context);
    // 簡易的なパラメータ予測（ダミー）
    return "{\"value\": 0.5}";
}

bool LlamaLocalAgent::requiresCloudEscalation(const QString& userPrompt, const AIContext& context) {
    Q_UNUSED(context);
    
    // 簡易的な判定ロジック
    // 「複雑なスクリプト」「新しいアルゴリズム」などの単語が含まれていたらクラウドへ
    if (userPrompt.contains("script") || userPrompt.contains("algorithm") || 
        userPrompt.contains("coding") || userPrompt.contains("映画") || 
        userPrompt.contains("cinematic")) {
        return true;
    }
    return false;
}

LocalAnalysisResult LlamaLocalAgent::analyzeUserQuestion(const QString& question, const AIContext& context) {
    LocalAnalysisResult result;
    
    // 1. 質問の意図をキーワードマッチングで分類
    const QString q = question.toLower();
    
    if (q.contains("見え") || q.contains("表示") || q.contains("visible") || 
        q.contains("display") || q.contains("show")) {
        result.intent = "visibility";
        result.requiredData = {
            "layer.visible",
            "layer.opacity",
            "layer.inPoint",
            "layer.outPoint",
            "layer.solo",
            "layer.parent",
            "layer.locked"
        };
        result.confidence = 0.9f;
    } else if (q.contains("アニメ") || q.contains("動き") || q.contains("motion") || 
               q.contains("animate") || q.contains("keyframe")) {
        result.intent = "animation";
        result.requiredData = {
            "layer.keyframes",
            "layer.properties",
            "layer.inPoint",
            "layer.outPoint"
        };
        result.confidence = 0.85f;
    } else if (q.contains("カラー") || q.contains("色") || q.contains("color") || 
               q.contains("grade") || q.contains("lumetri")) {
        result.intent = "color";
        result.requiredData = {
            "composition.colorSpace",
            "layer.effects",
            "layer.adjustmentLayer"
        };
        result.confidence = 0.85f;
    } else if (q.contains("オーディオ") || q.contains("音声") || q.contains("audio") || 
               q.contains("sound") || q.contains("volume")) {
        result.intent = "audio";
        result.requiredData = {
            "layer.audioLevels",
            "layer.mute",
            "composition.sampleRate"
        };
        result.confidence = 0.85f;
    } else if (q.contains("レンダー") || q.contains("出力") || q.contains("render") || 
               q.contains("export") || q.contains("encode")) {
        result.intent = "render";
        result.requiredData = {
            "composition.resolution",
            "composition.frameRate",
            "renderQueue.status"
        };
        result.confidence = 0.8f;
    } else if (q.contains("エフェクト") || q.contains("effect") || 
               q.contains("filter") || q.contains("plugin")) {
        result.intent = "effect";
        result.requiredData = {
            "layer.effects",
            "effect.parameters",
            "effect.enabled"
        };
        result.confidence = 0.85f;
    } else {
        result.intent = "unknown";
        result.confidence = 0.5f;
    }
    
    // 2. エンティティ抽出（レイヤー名、プロパティ名など）
    QRegularExpression layerNameRe(R"((?:レイヤー | layer)[「\"']([^「」\"']+)[」\"'])");
    auto match = layerNameRe.match(question);
    if (match.hasMatch()) {
        result.entities["layerName"] = match.captured(1);
    }
    
    // 3. 必要なデータを収集して要約
    QString collectedData;
    
    // 選択中のレイヤー情報を取得（AIContext から）
    auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        collectedData += QString("選択中のレイヤー：%1\n").arg(selectedLayers.front());
        
        for (const auto& dataKey : result.requiredData) {
            // AIContext からはレイヤー名のみ取得可能
            // 詳細なプロパティは実際のレイヤーオブジェクトから取得する必要がある
            if (dataKey == "layer.visible") {
                collectedData += "  表示：(要確認)\n";
            } else if (dataKey == "layer.opacity") {
                collectedData += "  不透明度：(要確認)\n";
            } else if (dataKey == "layer.inPoint") {
                collectedData += "  IN ポイント：(要確認)\n";
            } else if (dataKey == "layer.outPoint") {
                collectedData += "  OUT ポイント：(要確認)\n";
            } else if (dataKey == "layer.solo") {
                collectedData += "  ソロ：(要確認)\n";
            } else if (dataKey == "layer.locked") {
                collectedData += "  ロック：(要確認)\n";
            }
        }
    }
    
    QString activeCompId = context.activeCompositionId();
    if (!activeCompId.isEmpty()) {
        collectedData += QString("アクティブコンポジション：%1\n").arg(activeCompId);
    }
    
    // 4. クラウド要不要を判断
    if (result.intent == "visibility") {
        // 表示問題はローカルで対処可能
        result.requiresCloud = false;
        result.localAnswer = impl_->generateResponse(result.intent, collectedData);
    } else if (result.intent == "animation" && (q.contains("自動") || q.contains("auto"))) {
        // 自動アニメーションは複雑 → クラウドへ
        result.requiresCloud = true;
        result.summarizedContext = filterSensitiveInfo(collectedData);
    } else if (result.intent == "color" && (q.contains("映画") || q.contains("cinematic") || q.contains("look"))) {
        // 映画的カラーグレーディングは複雑 → クラウドへ
        result.requiresCloud = true;
        result.summarizedContext = filterSensitiveInfo(collectedData + "\n要求：映画風カラーグレーディング");
    } else if (result.intent == "effect" && (q.contains("作成") || q.contains("create") || q.contains("custom"))) {
        // カスタムエフェクト作成は複雑 → クラウドへ
        result.requiresCloud = true;
        result.summarizedContext = filterSensitiveInfo(collectedData + "\n要求：カスタムエフェクト作成");
    } else {
        // デフォルトはローカルで回答
        result.requiresCloud = false;
        result.localAnswer = impl_->generateResponse(result.intent, collectedData);
    }
    
    // 5. 機密情報をフィルタリング
    result.summarizedContext = filterSensitiveInfo(result.summarizedContext.isEmpty() ? collectedData : result.summarizedContext);
    
    return result;
}

QString LlamaLocalAgent::filterSensitiveInfo(const QString& text) {
    if (text.isEmpty()) return text;
    
    QString filtered = text;
    
    // ファイルパスを匿名化
    filtered.replace(QRegularExpression(R"(C:\\Users\\[^\\]+\\)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"(/Users/[^/]+/)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"([A-Z]:\\[^\\]+\\)"), "[PATH]/");
    
    // プロジェクト名を匿名化
    filtered.replace(QRegularExpression(R"(\[プロジェクト：[^\]]+\])"), "[プロジェクト]");
    filtered.replace(QRegularExpression(R"(プロジェクト「[^」]+」)"), "プロジェクト");
    
    // ユーザー名を匿名化
    filtered.replace(QRegularExpression(R"(user: [^\n]+)"), "user: [ANONYMOUS]");
    
    return filtered;
}

void LlamaLocalAgent::setMaxTokens(int maxTokens) { impl_->n_predict = maxTokens; }
void LlamaLocalAgent::setTemperature(float temperature) { impl_->temp = temperature; }

} // namespace ArtifactCore
