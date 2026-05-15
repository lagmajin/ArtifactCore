module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <vector>
#include <complex>
#include <memory>
#include <string>

export module Audio.Analyze;

import Audio.Segment;

export namespace ArtifactCore {

class LIBRARY_DLL_API AudioAnalyzer {
public:
    struct AnalysisResult {
        float rms = 0.0f;
        float peak = 0.0f;
        std::vector<float> spectrum;
        float lowIntensity = 0.0f;
        float midIntensity = 0.0f;
        float highIntensity = 0.0f;
    };

    AudioAnalyzer(int fftSize = 1024);
    ~AudioAnalyzer();

    // 解析の実行
    AnalysisResult analyze(const AudioSegment& segment);

    // FFTサイズの設定
    void setFFTSize(int size);
    int getFFTSize() const { return fftSize_; }

private:
    void computeFFT(std::vector<std::complex<float>>& data);
    float getIntensity(const std::vector<float>& spectrum, float freqStart, float freqEnd, int sampleRate);

    int fftSize_ = 1024;
    std::vector<float> window_;
    void initWindow();
};

} // namespace ArtifactCore
