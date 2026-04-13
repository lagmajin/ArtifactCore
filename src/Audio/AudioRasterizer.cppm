module;
#include <utility>
#include <algorithm>
#include <cmath>

module Audio.Rasterizer;

import std;
import Audio.Segment;

namespace ArtifactCore {

// ─────────────────────────────────────────────
// AudioRasterizer 実装
// ─────────────────────────────────────────────

class AudioRasterizer::Impl {
public:
    int displayWidth_ = 0;
    int displayHeight_ = 0;
    bool useRMS_ = true;
    int channelsToMerge_ = 2;  // Stereo → Mono
};

AudioRasterizer::AudioRasterizer() : impl_(new Impl()) {}
AudioRasterizer::~AudioRasterizer() { delete impl_; }

void AudioRasterizer::setDisplaySize(int width, int height) {
    impl_->displayWidth_ = width;
    impl_->displayHeight_ = height;
}

void AudioRasterizer::setUseRMS(bool use) {
    impl_->useRMS_ = use;
}

WaveformData AudioRasterizer::rasterize(const AudioSegment& segment) {
    WaveformData data;

    if (segment.channelData.isEmpty() || impl_->displayWidth_ <= 0) {
        return data;
    }

    // モノラルミックス
    const int numChannels = segment.channelCount();
    const int numSamples = segment.frameCount();
    if (numSamples == 0) {
        return data;
    }

    QVector<float> monoSamples(numSamples, 0.0f);
    for (int ch = 0; ch < numChannels; ++ch) {
        const auto& chData = segment.channelData[ch];
        const int n = std::min(numSamples, static_cast<int>(chData.size()));
        for (int i = 0; i < n; ++i) {
            monoSamples[i] += chData[i];
        }
    }
    if (numChannels > 1) {
        const float inv = 1.0f / static_cast<float>(numChannels);
        for (int i = 0; i < numSamples; ++i) {
            monoSamples[i] *= inv;
        }
    }

    // ピクセルごとのmin/maxを計算
    const int samplesPerPixel = std::max(1, numSamples / impl_->displayWidth_);
    data.minValues.resize(impl_->displayWidth_);
    data.maxValues.resize(impl_->displayWidth_);

    for (int px = 0; px < impl_->displayWidth_; ++px) {
        const int start = px * samplesPerPixel;
        const int end = std::min(start + samplesPerPixel, numSamples);

        if (start >= numSamples) {
            data.minValues[px] = 0.0f;
            data.maxValues[px] = 0.0f;
            continue;
        }

        float minVal = 0.0f;
        float maxVal = 0.0f;
        float sumSquares = 0.0f;
        int count = 0;

        for (int s = start; s < end; ++s) {
            const float val = monoSamples[s];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
            if (impl_->useRMS_) {
                sumSquares += val * val;
                ++count;
            }
        }

        data.minValues[px] = minVal;
        data.maxValues[px] = maxVal;

        // RMS値も計算（必要に応じて拡張可能）
        if (impl_->useRMS_ && count > 0) {
            float rms = std::sqrt(sumSquares / static_cast<float>(count));
            // min/max を RMS でクリップすることも可能
        }
    }

    return data;
}

WaveformData AudioRasterizer::rasterizeRange(const AudioSegment& segment,
                                              int startSample,
                                              int sampleCount) {
    if (startSample < 0 || sampleCount <= 0) {
        return WaveformData();
    }

    // 部分セグメントを作成
    AudioSegment sub;
    sub.sampleRate = segment.sampleRate;
    sub.layout = segment.layout;
    sub.startFrame = segment.startFrame + startSample;

    const int numChannels = segment.channelCount();
    sub.channelData.resize(numChannels);

    for (int ch = 0; ch < numChannels; ++ch) {
        const auto& chData = segment.channelData[ch];
        const int actualStart = std::min(startSample, static_cast<int>(chData.size()));
        const int actualCount = std::min(sampleCount, static_cast<int>(chData.size()) - actualStart);
        if (actualCount > 0) {
            sub.channelData[ch] = chData.mid(actualStart, actualCount);
        }
    }

    return rasterize(sub);
}

QImage AudioRasterizer::renderToImage(const WaveformData& data) {
    if (data.minValues.isEmpty() || impl_->displayWidth_ <= 0 || impl_->displayHeight_ <= 0) {
        return QImage();
    }

    QImage image(impl_->displayWidth_, impl_->displayHeight_, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = impl_->displayWidth_;
    const int h = impl_->displayHeight_;
    const int centerY = h / 2;

    // 最大振幅を見つける
    float maxAbs = 0.001f;  // ゼロ除算防止
    for (int i = 0; i < data.minValues.size(); ++i) {
        maxAbs = std::max(maxAbs, std::abs(data.minValues[i]));
        maxAbs = std::max(maxAbs, std::abs(data.maxValues[i]));
    }

    const float scale = static_cast<float>(centerY - 2) / maxAbs;

    // 波形を描画
    painter.setPen(QPen(QColor(100, 160, 230), 1.0f));
    for (int x = 0; x < w && x < data.minValues.size(); ++x) {
        const float minV = data.minValues[x] * scale;
        const float maxV = data.maxValues[x] * scale;

        painter.drawLine(x, centerY - maxV, x, centerY - minV);
    }

    return image;
}

} // namespace ArtifactCore
