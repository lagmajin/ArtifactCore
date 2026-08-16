module;
#include <algorithm>
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <limits>
#include <QtGlobal>
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

    String getName() const override { return "Audio Spectrum"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // 分析結果取得
    const std::vector<float>& getSpectrum() const { return spectrum_; }
    const std::vector<float>& getWaveform() const { return waveform_; }

    // Loudness metering (linear PCM, LUFS approximation).
    // The values are updated by process() and are expressed in LUFS.
    float getMomentaryLufs() const { return momentaryLufs_; }
    float getShortTermLufs() const { return shortTermLufs_; }
    float getIntegratedLufs() const { return integratedLufs_; }
    float getLoudnessRangeLufs() const { return loudnessRangeLufs_; }
    float getPeakDb() const { return peakDb_; }
    // 4x linear-interpolated true-peak approximation; not ITU-R BS.1770 oversampling.
    float getTruePeakDb() const { return truePeakDb_; }
    // Clears the time-based loudness state without changing FFT settings.
    void resetLoudnessMeasurement();
    float normalizationGainDb(float targetLufs) const;
    bool normalizeToTargetLufs(AudioSegment& segment, float targetLufs);
    
    void setBins(int bins) {
        bins_ = bins > 0 ? std::min(bins, 1 << 20) : 1;
    }
    int getBins() const { return bins_; }

private:
    int bins_ = 64;
    
    // 分析結果（スレッド間共有）
    std::atomic<bool> spectrumReady_{false};
    std::vector<float> spectrum_;
    std::vector<float> waveform_;
    float momentaryLufs_ = -std::numeric_limits<float>::infinity();
    float shortTermLufs_ = -std::numeric_limits<float>::infinity();
    float integratedLufs_ = -std::numeric_limits<float>::infinity();
    float loudnessRangeLufs_ = 0.0f;
    float peakDb_ = -std::numeric_limits<float>::infinity();
    float truePeakDb_ = -std::numeric_limits<float>::infinity();
    double integratedEnergySum_ = 0.0;
    qint64 integratedFrameCount_ = 0;
    qint64 lastEndFrame_ = -1;

    struct LoudnessWindow {
        qint64 startFrame = 0;
        qint64 frameCount = 0;
        double meanSquare = 0.0;
        float lufs = -std::numeric_limits<float>::infinity();
    };
    std::vector<LoudnessWindow> loudnessWindows_;
    
    // FFT（簡易実装）
    void computeFFT(const std::vector<float>& input, std::vector<float>& output);
};

} // namespace ArtifactCore
