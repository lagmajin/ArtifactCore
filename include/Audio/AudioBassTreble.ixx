module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.BassTreble;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Bass & Treble エフェクト
 * 低音帯（Bass）と高音帯（Treble）を個別にブースト/カットします。
 * After Effects 相当の基本EQエフェクトとして機能します。
 */
class LIBRARY_DLL_API AudioBassTreble : public AudioEffect {
public:
    AudioBassTreble();
    virtual ~AudioBassTreble() = default;

    String getName() const override { return "Bass & Treble"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ（dB単位）
    void setBass(float db) { bassDb_ = std::isfinite(db) ? std::clamp(db, -24.0f, 24.0f) : 0.0f; }
    void setTreble(float db) { trebleDb_ = std::isfinite(db) ? std::clamp(db, -24.0f, 24.0f) : 0.0f; }
    
    float getBass() const { return bassDb_; }
    float getTreble() const { return trebleDb_; }

private:
    float bassDb_ = 0.0f;    // -24dB ~ +24dB
    float trebleDb_ = 0.0f;  // -24dB ~ +24dB
    
    // 内部状態
    std::atomic<float> bassCoeff_{1.0f};
    std::atomic<float> trebleCoeff_{1.0f};
    std::vector<float> lowStates_;
    std::vector<float> highStates_;
    std::vector<bool> statesInitialized_;
    int stateSampleRate_ = 0;
};

} // namespace ArtifactCore
