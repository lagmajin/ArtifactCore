module;
#include <QString>
#include <QStringList>

module Core.AI.DescriptionExamples;

import std;
import Core.AI.Describable;

namespace ArtifactCore {

// ============================================================================
// 使用例1: クラス内で直接実装する方法
// ============================================================================

class ExampleLayer : public IDescribable {
    DECLARE_DESCRIPTION
    
public:
    // レイヤーの実際のメンバ
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }
    float opacity() const { return m_opacity; }
    void setOpacity(float op) { m_opacity = op; }
    
private:
    QString m_name;
    float m_opacity = 1.0f;
};

// 実装ファイル（.cppm）で説明を定義
IMPLEMENT_DESCRIPTION(ExampleLayer) {
    ClassDescription desc;
    desc.className = "ExampleLayer";
    desc.briefDescription = loc(
        "Base class for all layer types in the composition.",
        "コンポジション内の全レイヤータイプの基底クラス。",
        "合成中所有图层类型的基类。"
    );
    desc.detailedDescription = loc(
        "ExampleLayer provides common functionality for all layers including "
        "transform, opacity, blend modes, and effect stacking.",
        "ExampleLayerは、トランスフォーム、不透明度、ブレンドモード、"
        "エフェクトスタックなど、全レイヤー共通の機能を提供します。",
        "ExampleLayer为所有图层提供通用功能，包括变换、不透明度、"
        "混合模式和效果堆栈。"
    );
    desc.properties = {
        {"name", loc("Layer display name", "レイヤー表示名", "图层显示名称"), "QString"},
        {"opacity", loc("Layer opacity 0-1", "レイヤー不透明度 0-1", "图层不透明度0-1"), 
         "float", "1.0", "0.0", "1.0"},
        {"visible", loc("Whether layer is rendered", "レイヤーがレンダリングされるか", "图层是否渲染"), 
         "bool", "true"},
        {"blendMode", loc("How layer blends with below", "下のレイヤーとのブレンド方法", "与下图层的混合方式"), 
         "BlendMode", "Normal"}
    };
    desc.methods = {
        {"setName", loc("Set layer display name", "レイヤー表示名を設定", "设置图层显示名称"), 
         "void", {"QString"}, {"name"}},
        {"duplicate", loc("Create a copy of this layer", "このレイヤーのコピーを作成", "创建此图层的副本"), 
         "ExampleLayer*"}
    };
    desc.relatedClasses = {"ArtifactComposition", "ArtifactAbstractEffect"};
    desc.tags = {"layer", "composition", "rendering"};
    return desc;
}

// ============================================================================
// 使用例2: テンプレート特殊化で実装する方法（クラスを変更せずに説明を追加）
// ============================================================================

// 既存のクラス（変更不可と仮定）
class ExistingVideoDecoder {
public:
    QString getCodec() const;
    int getBitrate() const;
    void decodeFrame(int frameIndex);
};

// テンプレート特殊化で説明を追加
template<>
struct Describable<ExistingVideoDecoder> {
    static ClassDescription describe() {
        ClassDescription desc;
        desc.className = "ExistingVideoDecoder";
        desc.briefDescription = loc(
            "Video decoder for various codec formats.",
            "様々なコーデックフォーマット用の動画デコーダー。",
            "用于各种编解码格式的视频解码器。"
        );
        desc.properties = {
            {"codec", loc("Current codec name", "現在のコーデック名", "当前编解码器名称"), "QString"},
            {"bitrate", loc("Video bitrate in kbps", "動画ビットレート（kbps）", "视频比特率（kbps）"), "int"}
        };
        desc.methods = {
            {"decodeFrame", loc("Decode single frame", "単一フレームをデコード", "解码单帧"), 
             "void", {"int"}, {"frameIndex"}}
        };
        desc.tags = {"video", "codec", "decoding"};
        return desc;
    }
};

// 自動登録
REGISTER_DESCRIPTION(ExistingVideoDecoder)

// ============================================================================
// 使用例3: 継承を使わずに静的メソッドだけで提供
// ============================================================================

class ArtifactUtility {
public:
    // クラスの実際の静的メソッド
    static QString version();
    static void initialize();
    
    // 説明を返す静的メソッド
    static ClassDescription describe() {
        return {
            "ArtifactUtility",
            loc("Utility functions for the Artifact application.",
                "Artifactアプリケーション用のユーティリティ関数。",
                "Artifact应用程序的实用函数。"),
            {}, // detailedDescription
            {
                {"version", loc("Application version string", "アプリケーションバージョン文字列", "应用程序版本字符串"), "QString"}
            },
            {
                {"initialize", loc("Initialize application systems", "アプリケーションシステムを初期化", "初始化应用程序系统"), "void"}
            },
            {}, // relatedClasses
            {"utility", "system"}
        };
    }
};

} // namespace ArtifactCore