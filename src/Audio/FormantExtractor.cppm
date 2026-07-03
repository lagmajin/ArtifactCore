module;
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

module Audio.FormantExtractor;

import Audio.Analyze;

namespace ArtifactCore {

FormantExtractor::FormantExtractor() = default;
FormantExtractor::~FormantExtractor() = default;

// ─────────────────────────────────────────────────────────
// スペクトルピーク検出
// ─────────────────────────────────────────────────────────
std::vector<float> FormantExtractor::findPeaks(
    const std::vector<float>& spectrum,
    float binSize, float minFreq, float maxFreq)
{
    std::vector<float> peaks;
    if (spectrum.empty() || binSize <= 0.0f) return peaks;

    int startBin = std::max(1, static_cast<int>(minFreq / binSize));
    int endBin = std::min(static_cast<int>(spectrum.size() - 1),
                          static_cast<int>(maxFreq / binSize));

    for (int i = startBin; i <= endBin; ++i) {
        if (spectrum[i] > spectrum[i - 1] && spectrum[i] > spectrum[i + 1]) {
            if (spectrum[i] > 0.01f) {
                peaks.push_back(static_cast<float>(i) * binSize);
            }
        }
    }

    // 振幅順にソートして上位3つをフォルマント候補とする
    std::sort(peaks.begin(), peaks.end(),
              [&spectrum, binSize](float a, float b) {
                  int ia = static_cast<int>(a / binSize);
                  int ib = static_cast<int>(b / binSize);
                  if (ia < 0 || ia >= static_cast<int>(spectrum.size())) return false;
                  if (ib < 0 || ib >= static_cast<int>(spectrum.size())) return true;
                  return spectrum[ia] > spectrum[ib];
              });

    if (peaks.size() > 3) peaks.resize(3);
    std::sort(peaks.begin(), peaks.end());

    return peaks;
}
// ─────────────────────────────────────────────────────────
// フォルマント抽出
// ─────────────────────────────────────────────────────────
FormantSet FormantExtractor::extractFormants(const AudioSegment& segment)
{
    FormantSet result;
    AudioAnalyzer analyzer(2048);
    auto analysis = analyzer.analyze(segment);
    if (analysis.spectrum.empty()) return result;

    result.intensity = analysis.rms;

    int n = static_cast<int>(analysis.spectrum.size()) * 2;
    float binSize = static_cast<float>(segment.sampleRate) / (2.0f * (n - 1));
    if (binSize <= 0.0f) return result;

    auto peaks = findPeaks(analysis.spectrum, binSize, f1Min_, f2Max_ + 500.0f);

    for (float freq : peaks) {
        if (freq >= f1Min_ && freq <= f1Max_ && result.f1 == 0.0f)
            result.f1 = freq;
        else if (freq >= f2Min_ && freq <= f2Max_ && result.f2 == 0.0f)
            result.f2 = freq;
        else if (result.f3 == 0.0f)
            result.f3 = freq;
    }
    return result;
}
// ─────────────────────────────────────────────────────────
// 母音分類（F1/F2 フォルマント空間での距離判定）
// ─────────────────────────────────────────────────────────
PhonemeLabel FormantExtractor::classifyPhoneme(const FormantSet& formants) const
{
    if (formants.intensity < 0.01f) return PhonemeLabel::Silence;

    if (formants.f1 < f1Min_ && formants.f2 < f2Min_) {
        if (formants.f3 > 3000.0f) return PhonemeLabel::F;
        if (formants.intensity < 0.05f) return PhonemeLabel::M;
        return PhonemeLabel::N;
    }

    const float f1 = formants.f1;
    const float f2 = formants.f2;

    struct VowelRef { float f1, f2; PhonemeLabel label; };
    static const VowelRef vowels[] = {
        {800, 1200, PhonemeLabel::A},
        {300, 2300, PhonemeLabel::I},
        {350, 1300, PhonemeLabel::U},
        {500, 1800, PhonemeLabel::E},
        {450,  900, PhonemeLabel::O},
    };

    PhonemeLabel bestLabel = PhonemeLabel::Other;
    float bestDist = 1e10f;
    for (const auto& v : vowels) {
        float d = (f1 - v.f1) * (f1 - v.f1) + (f2 - v.f2) * (f2 - v.f2);
        if (d < bestDist) { bestDist = d; bestLabel = v.label; }
    }
    return bestLabel;
}
// ─────────────────────────────────────────────────────────
// 1フレーム解析
// ─────────────────────────────────────────────────────────
PhonemeEvent FormantExtractor::analyzeFrame(
    const AudioSegment& segment, int64_t frame)
{
    PhonemeEvent event;
    event.frame = frame;
    auto formants = extractFormants(segment);
    event.label = classifyPhoneme(formants);
    event.intensity = formants.intensity;
    event.confidence = (formants.f1 > 0.0f && formants.f2 > 0.0f) ? 0.7f : 0.3f;
    return event;
}

// ─────────────────────────────────────────────────────────
// トラック全体の連続解析
// ─────────────────────────────────────────────────────────
std::vector<PhonemeEvent> FormantExtractor::analyzeTrack(
    const AudioSegment& segment, double frameRate, int64_t startFrame)
{
    std::vector<PhonemeEvent> events;
    if (segment.frameCount() <= 0 || frameRate <= 0.0) return events;

    int sampleRate = segment.sampleRate;
    int framesPerAnalysis = static_cast<int>(std::round(sampleRate / frameRate));
    if (framesPerAnalysis <= 0) framesPerAnalysis = sampleRate / 24;

    int totalFrames = segment.frameCount();
    int64_t frame = startFrame;

    for (int offset = 0; offset + framesPerAnalysis <= totalFrames;
         offset += framesPerAnalysis, ++frame) {
        AudioSegment frameSeg;
        frameSeg.sampleRate = sampleRate;
        frameSeg.channelData.resize(segment.channelCount());
        for (int ch = 0; ch < segment.channelCount(); ++ch) {
            const float* src = segment.constData(ch);
            if (!src) continue;
            frameSeg.channelData[ch].resize(framesPerAnalysis);
            std::copy(src + offset, src + offset + framesPerAnalysis,
                      frameSeg.channelData[ch].begin());
        }
        frameSeg.startFrame = offset;
        auto event = analyzeFrame(frameSeg, frame);
        events.push_back(event);
    }
    return events;
}

void FormantExtractor::setVowelThresholds(
    float f1min, float f1max, float f2min, float f2max)
{
    f1Min_ = f1min; f1Max_ = f1max;
    f2Min_ = f2min; f2Max_ = f2max;
}

} // namespace ArtifactCore
