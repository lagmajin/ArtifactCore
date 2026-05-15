module;
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect;

import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief 全てのオーディオエフェクトの基底クラス
 * 内部エフェクトや VST プラグインのラッパーとして機能します。
 */
class LIBRARY_DLL_API AudioEffect {
public:
    virtual ~AudioEffect() = default;

    virtual std::string getName() const = 0;
    
    /**
     * @brief 音声処理の実行 (インプレース)
     * @param segment 処理対象のメインバッファ
     * @param sideChain サイドチェーン参照用バッファ (任意)
     */
    virtual void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) = 0;

    virtual void setBypass(bool bypass) { bypass_ = bypass; }
    virtual bool isBypassed() const { return bypass_; }

protected:
    bool bypass_ = false;
};

} // namespace ArtifactCore
