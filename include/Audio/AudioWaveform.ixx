module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Waveform;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Audio Waveform エフェクト
 * 音声の波形形状を抽出します。
 * After Effects Audio Waveform 生成用。
 */
class LIBRARY_DLL_API AudioWaveform : public AudioEffect {
public:
    AudioWaveform();
    virtual ~AudioWaveform() = default;

    std::string getName() const override { return "Audio Waveform"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // 波形取得
    const std::vector<float>& getWaveformData() const { return waveformData_; }
    int getResolution() const { return resolution_; }
    void setResolution(int res) { resolution_ = res; }

private:
    int resolution_ = 512;
    std::vector<float> waveformData_;
};

} // namespace ArtifactCore