module;
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numeric>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QList>
module Audio.Analyze;





namespace ArtifactCore {

AudioAnalyzer::AudioAnalyzer(int fftSize) : fftSize_(fftSize) {
    initWindow();
}

AudioAnalyzer::~AudioAnalyzer() = default;

void AudioAnalyzer::initWindow() {
    window_.resize(fftSize_);
    // Hamming window
    for (int i = 0; i < fftSize_; ++i) {
        window_[i] = 0.54f - 0.46f * static_cast<float>(std::cos(2.0 * 3.14159265358979323846 * i / (fftSize_ - 1)));
    }
}

void AudioAnalyzer::setFFTSize(int size) {
    if (fftSize_ != size) {
        fftSize_ = size;
        initWindow();
    }
}

// シンプルな Radix-2 FFT (Cooley-Tukey)
void AudioAnalyzer::computeFFT(std::vector<std::complex<float>>& data) {
    int n = static_cast<int>(data.size());
    if (n <= 1) return;

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    // Cooley-Tukey
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * 3.1415926535f / len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1);
            for (int j = 0; j < len / 2; j++) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

float AudioAnalyzer::getIntensity(const std::vector<float>& spectrum, float freqStart, float freqEnd, int sampleRate) {
    if (spectrum.empty()) return 0.0f;
    
    int n = static_cast<int>(spectrum.size());
    float binSize = static_cast<float>(sampleRate) / (2.0f * (n - 1));
    
    int startIdx = static_cast<int>(freqStart / binSize);
    int endIdx = static_cast<int>(freqEnd / binSize);
    
    startIdx = std::clamp(startIdx, 0, n - 1);
    endIdx = std::clamp(endIdx, startIdx, n - 1);
    
    float sum = 0.0f;
    for (int i = startIdx; i <= endIdx; ++i) {
        sum += spectrum[i];
    }
    
    return (endIdx > startIdx) ? sum / (endIdx - startIdx + 1) : spectrum[startIdx];
}

AudioAnalyzer::AnalysisResult AudioAnalyzer::analyze(const AudioSegment& segment) {
    AnalysisResult result;
    if (segment.channelData.isEmpty()) return result;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    if (frames == 0) return result;

    // 1. RMS と Peak の計算
    float sumSq = 0.0f;
    float maxAbs = 0.0f;
    
    // ステレオならミックスして処理するか、全チャンネル平均
    std::vector<float> monoData(frames, 0.0f);
    for (int c = 0; c < channels; ++c) {
        const float* data = segment.constData(c);
        for (int i = 0; i < frames; ++i) {
            float s = data[i];
            monoData[i] += s;
            sumSq += s * s;
            maxAbs = std::max(maxAbs, std::abs(s));
        }
    }
    
    result.rms = std::sqrt(sumSq / (frames * channels));
    result.peak = maxAbs;

    // 2. FFT解析 (モノラルミックスで行う)
    int n = fftSize_;
    std::vector<std::complex<float>> fftData(n, 0.0f);
    
    // データのコピーと窓関数の適用
    int copyLen = std::min(frames, n);
    float invChannels = 1.0f / channels;
    for (int i = 0; i < copyLen; ++i) {
        fftData[i] = std::complex<float>(monoData[i] * invChannels * window_[i], 0.0f);
    }
    
    computeFFT(fftData);
    
    // スペクトル強度の算出 (マグニチュード)
    result.spectrum.resize(n / 2 + 1);
    for (int i = 0; i <= n / 2; ++i) {
        result.spectrum[i] = std::abs(fftData[i]) / (n / 2);
    }

    // 3. 帯域ごとの強度算出
    // Low: 0-250Hz, Mid: 250-2500Hz, High: 2500Hz-
    result.lowIntensity = getIntensity(result.spectrum, 0.0f, 250.0f, segment.sampleRate);
    result.midIntensity = getIntensity(result.spectrum, 250.0f, 2500.0f, segment.sampleRate);
    result.highIntensity = getIntensity(result.spectrum, 2500.0f, 20000.0f, segment.sampleRate);

    return result;
}

} // namespace ArtifactCore
