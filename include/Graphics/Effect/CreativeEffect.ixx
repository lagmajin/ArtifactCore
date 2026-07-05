module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "../Define/DllExportMacro.hpp"

export module Graphics.Effect.Creative;

import Channel;

export namespace ArtifactCore {

/**
 * @brief エフェクトパラメータの型定義
 */
enum class EffectParameterType {
    Float,
    Int,
    Bool,
    Vector2,
    Vector3,
    Color,
    Texture
};

/**
 * @brief 単一のエフェクトパラメータ
 */
struct LIBRARY_DLL_API EffectParameter {
    std::string name;
    std::string displayName;
    EffectParameterType type;
    std::variant<float, int, bool, std::vector<float>> value;
    float minValue = 0.0f;
    float maxValue = 1.0f;
};

/**
 * @brief エフェクト実行時の情報を保持する構造体
 */
struct LIBRARY_DLL_API CreativeEffectContext {
    double time = 0.0;           // 現在の再生時間 (sec)
    int frameIndex = 0;          // 現在のフレーム番号
    float frameRate = 30.0f;     // フレームレート
    int renderWidth = 1920;      // ターゲット解像度
    int renderHeight = 1080;
    
    // 他の AOV (深度など) が必要か等のフラグ
    bool needsDepth = false;
    bool needsVelocity = false;
};

/**
 * @brief クリエイティブ画像エフェクトの基底クラス
 * After Effects のプラグインのように、VideoFrame に対して非破壊的な加工を行います。
 */
class LIBRARY_DLL_API CreativeEffect {
public:
    virtual ~CreativeEffect() = default;

    virtual std::string getName() const = 0;
    virtual std::string getCategory() const { return "General"; }

    /**
     * @brief エフェクトの適用開始 (初期化)
     */
    virtual bool initialize() { return true; }

    /**
     * @brief エフェクトの実行 (CPU実装)
     * @param frame 入出力兼用のビデオフレーム (マルチチャンネル対応)
     * @param context 実行時のコンテキスト情報
     */
    virtual void process(VideoFrame& frame, const CreativeEffectContext& context) = 0;

    /**
     * @brief パラメータ管理
     */
    virtual std::vector<EffectParameter>& getParameters() { return parameters_; }
    virtual void setParameter(const std::string& name, float value);
    virtual float getParameter(const std::string& name) const;

    virtual bool isEnabled() const { return enabled_; }
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }

protected:
    std::vector<EffectParameter> parameters_;
    bool enabled_ = true;
};

} // namespace ArtifactCore
