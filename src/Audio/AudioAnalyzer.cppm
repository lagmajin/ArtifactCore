module;
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>
#include <QtGlobal>

module Audio.Analyze;


import Container.NamedVector;

namespace ArtifactCore {

namespace {
constexpr int kMaxFFTSize = 1 << 20;

int normalizeFFTSize(int size) {
    size = std::clamp(size, 2, kMaxFFTSize);
    int normalized = 1;
    while (normalized < size && normalized < kMaxFFTSize) {
        normalized <<= 1;
    }
    return normalized;
}

float finiteOrZero(const float value)
{
    return std::isfinite(value) ? value : 0.0f;
}
}

AudioAnalyzer::AudioAnalyzer(int fftSize) : fftSize_(normalizeFFTSize(fftSize)) {
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
    size = normalizeFFTSize(size);
    if (fftSize_ != size) {
        fftSize_ = size;
        initWindow();
    }
}

// シンプルな Radix-2 FFT (Cooley-Tukey)
void AudioAnalyzer::computeFFT(std::vector<std::complex<double>>& data) {
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
        const double ang = 2.0 * 3.14159265358979323846 / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; j++) {
                std::complex<double> u = data[i + j];
                std::complex<double> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

float AudioAnalyzer::getIntensity(const std::vector<float>& spectrum, float freqStart, float freqEnd, int sampleRate) {
    if (spectrum.empty() || sampleRate <= 0 ||
        !std::isfinite(freqStart) || !std::isfinite(freqEnd) ||
        freqStart > freqEnd) return 0.0f;
    
    int n = static_cast<int>(spectrum.size());
    float binSize = static_cast<float>(sampleRate) / (2.0f * (n - 1));
    if (!std::isfinite(binSize) || binSize <= 0.0f) return 0.0f;
    
    const auto frequencyToIndex = [binSize, n](float frequency) {
        const double rawIndex = static_cast<double>(frequency) /
                                static_cast<double>(binSize);
        if (!std::isfinite(rawIndex) || rawIndex >= n) {
            return n - 1;
        }
        if (rawIndex <= 0.0) {
            return 0;
        }
        return static_cast<int>(rawIndex);
    };
    int startIdx = frequencyToIndex(freqStart);
    int endIdx = frequencyToIndex(freqEnd);
    
    startIdx = std::clamp(startIdx, 0, n - 1);
    endIdx = std::clamp(endIdx, startIdx, n - 1);
    
    double sum = 0.0;
    for (int i = startIdx; i <= endIdx; ++i) {
        if (std::isfinite(spectrum[i])) {
            sum += spectrum[i];
        }
    }

    const double average = sum / (endIdx - startIdx + 1);
    return std::isfinite(average)
        ? static_cast<float>(average)
        : 0.0f;
}

AudioAnalyzer::AnalysisResult AudioAnalyzer::analyze(const AudioSegment& segment) {
    AnalysisResult result;
    if (segment.channelData.isEmpty()) return result;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    if (frames == 0) return result;

    // 1. RMS と Peak の計算
    double sumSq = 0.0;
    float maxAbs = 0.0f;
    
    // ステレオならミックスして処理するか、全チャンネル平均
    NamedVector<float> monoData{makeNamedVector<float>(ContainerName{"AudioAnalyzerMonoData"})};
    monoData.resize(static_cast<std::size_t>(frames));
    for (int c = 0; c < channels; ++c) {
        const float* data = segment.constData(c);
        const int availableFrames = data
            ? static_cast<int>(std::min<qsizetype>(
                  static_cast<qsizetype>(frames), segment.channelData[c].size()))
            : 0;
        for (int i = 0; i < availableFrames; ++i) {
            const float s = data[i];
            if (!std::isfinite(s)) continue;
            const float mixed = *monoData.at(static_cast<std::size_t>(i)) + s;
            *monoData.at(static_cast<std::size_t>(i)) = std::isfinite(mixed)
                ? mixed
                : std::copysign(std::numeric_limits<float>::max(), mixed);
            sumSq += static_cast<double>(s) * s;
            maxAbs = std::max(maxAbs, std::abs(s));
        }
    }

    const double sampleCount = static_cast<double>(frames) * channels;
    if (sampleCount <= 0.0 || !std::isfinite(sampleCount)) return result;
    const double rms = std::sqrt(sumSq / sampleCount);
    result.rms = std::isfinite(rms)
        ? static_cast<float>(std::min(
              rms, static_cast<double>(std::numeric_limits<float>::max())))
        : 0.0f;
    result.peak = finiteOrZero(maxAbs);

    // 2. FFT解析 (モノラルミックスで行う)
    int n = fftSize_;
    std::vector<std::complex<double>> fftData(n, 0.0);
    
    // データのコピーと窓関数の適用
    int copyLen = std::min(frames, n);
    float invChannels = 1.0f / channels;
    for (int i = 0; i < copyLen; ++i) {
        fftData[i] = std::complex<double>(
            static_cast<double>(*monoData.at(static_cast<std::size_t>(i))) *
                invChannels * window_[i],
            0.0);
    }
    
    computeFFT(fftData);
    
    // スペクトル強度の算出 (マグニチュード)
    result.spectrum.resize(n / 2 + 1);
    for (int i = 0; i <= n / 2; ++i) {
        const double magnitude = std::abs(fftData[i]) / (n / 2);
        result.spectrum[i] = std::isfinite(magnitude)
            ? static_cast<float>(std::min(
                magnitude, static_cast<double>(std::numeric_limits<float>::max())))
            : 0.0f;
    }

    // 3. 帯域ごとの強度算出
    // Low: 0-250Hz, Mid: 250-2500Hz, High: 2500Hz-
    result.lowIntensity = getIntensity(result.spectrum, 0.0f, 250.0f, segment.sampleRate);
    result.midIntensity = getIntensity(result.spectrum, 250.0f, 2500.0f, segment.sampleRate);
    result.highIntensity = getIntensity(result.spectrum, 2500.0f, 20000.0f, segment.sampleRate);

    return result;
}

} // namespace ArtifactCore
