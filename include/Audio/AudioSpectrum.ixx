module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Spectrum;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Audio Spectrum エフェクト
 * 周波数スペクトラム分析を行い、結果を保存します。
 * After Effects Audio Spectrum/Waveform 生成用。
 */
class LIBRARY_DLL_API AudioSpectrum : public AudioEffect {
public:
    AudioSpectrum();
    virtual ~AudioSpectrum() = default;

    std::string getName() const override { return "Audio Spectrum"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // 分析結果取得
    const std::vector<float>& getSpectrum() const { return spectrum_; }
    const std::vector<float>& getWaveform() const { return waveform_; }
    
    void setBins(int bins) { bins_ = bins; }
    int getBins() const { return bins_; }

private:
    int bins_ = 64;
    
    // 分析結果（スレッド間共有）
    std::atomic<bool> spectrumReady_{false};
    std::vector<float> spectrum_;
    std::vector<float> waveform_;
    
    // FFT（簡易実装）
    void computeFFT(const std::vector<float>& input, std::vector<float>& output);
};

} // namespace ArtifactCore