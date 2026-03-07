module;
#include <memory>
#include <string>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect;

import Audio.Segment;
import std;

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
     */
    virtual void process(AudioSegment& segment) = 0;

    virtual void setBypass(bool bypass) { bypass_ = bypass; }
    virtual bool isBypassed() const { return bypass_; }

protected:
    bool bypass_ = false;
};

} // namespace ArtifactCore
