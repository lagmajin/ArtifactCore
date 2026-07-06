module;
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>
#include <QString>

module Audio.LipSyncTrack;

import Audio.Analyze;
import Audio.FormantExtractor;
import Audio.SimpleWav;

namespace ArtifactCore {

namespace {

AudioSegment makeAudioSegmentFromWav(const SimpleWav& wav)
{
    AudioSegment segment;
    segment.sampleRate = wav.sampleRate();

    const int channels = std::max(0, wav.channelCount());
    const qint64 frames = std::max<qint64>(0, wav.frameCount());
    const QVector<float> pcm = wav.getAudioData();
    if (channels <= 0 || frames <= 0 || pcm.isEmpty()) {
        return segment;
    }

    switch (channels) {
    case 1:
        segment.layout = AudioChannelLayout::Mono;
        break;
    case 2:
        segment.layout = AudioChannelLayout::Stereo;
        break;
    case 6:
        segment.layout = AudioChannelLayout::Surround51;
        break;
    case 8:
        segment.layout = AudioChannelLayout::Surround71;
        break;
    case 10:
        segment.layout = AudioChannelLayout::Custom10ch;
        break;
    default:
        segment.layout = AudioChannelLayout::Custom10ch;
        break;
    }

    segment.channelData.resize(channels);
    for (int ch = 0; ch < channels; ++ch) {
        segment.channelData[ch].resize(static_cast<int>(frames));
    }

    const qint64 availableFrames = std::min(frames, static_cast<qint64>(pcm.size() / channels));
    for (qint64 frame = 0; frame < availableFrames; ++frame) {
        for (int ch = 0; ch < channels; ++ch) {
            const qint64 index = frame * channels + ch;
            if (index < pcm.size()) {
                segment.channelData[ch][static_cast<int>(frame)] = pcm[static_cast<int>(index)];
            }
        }
    }

    return segment;
}

} // namespace

LipSyncTrack::LipSyncTrack() = default;
LipSyncTrack::~LipSyncTrack() = default;

bool LipSyncTrack::analyze(const AudioSegment& segment, double frameRate)
{
    events_ = extractor_.analyzeTrack(segment, frameRate);
    return !events_.empty();
}

bool LipSyncTrack::analyzeFromFile(const QString& audioPath, double frameRate)
{
    const QString trimmed = audioPath.trimmed();
    if (trimmed.isEmpty() || frameRate <= 0.0) {
        events_.clear();
        return false;
    }

    SimpleWav wav;
    if (!wav.loadFromFile(trimmed)) {
        events_.clear();
        return false;
    }

    const AudioSegment segment = makeAudioSegmentFromWav(wav);
    if (segment.frameCount() <= 0 || segment.channelCount() <= 0) {
        events_.clear();
        return false;
    }

    return analyze(segment, frameRate);
}

std::vector<PhonemeEvent> LipSyncTrack::eventsInRange(
    int64_t startFrame, int64_t endFrame) const
{
    std::vector<PhonemeEvent> result;
    for (const auto& e : events_) {
        if (e.frame >= startFrame && e.frame <= endFrame) {
            result.push_back(e);
        }
    }
    return result;
}

PhonemeEvent LipSyncTrack::eventAtFrame(int64_t frame) const
{
    for (const auto& e : events_) {
        if (e.frame == frame) return e;
    }
    // 直前のイベントを探す
    PhonemeEvent last = {};
    for (const auto& e : events_) {
        if (e.frame <= frame) last = e;
        else break;
    }
    return last;
}

std::vector<int> LipSyncTrack::mouthShapeSequence() const
{
    std::vector<int> shapes;
    shapes.reserve(events_.size());
    for (const auto& e : events_) {
        shapes.push_back(e.mouthShapeIndex());
    }
    return shapes;
}

QJsonObject LipSyncTrack::toJson() const
{
    QJsonObject obj;
    QJsonArray arr;
    for (const auto& e : events_) {
        QJsonObject ev;
        ev["frame"] = static_cast<double>(e.frame);
        ev["label"] = static_cast<int>(e.label);
        ev["intensity"] = e.intensity;
        ev["confidence"] = e.confidence;
        arr.append(ev);
    }
    obj["events"] = arr;
    obj["mouthGroupId"] = mouthGroupId_;
    return obj;
}

void LipSyncTrack::fromJson(const QJsonObject& obj)
{
    events_.clear();
    mouthGroupId_ = obj.value("mouthGroupId").toInt(0);
    const QJsonArray arr = obj.value("events").toArray();
    for (const auto& val : arr) {
        QJsonObject ev = val.toObject();
        PhonemeEvent event;
        event.frame = static_cast<int64_t>(ev.value("frame").toDouble(0.0));
        event.label = static_cast<PhonemeLabel>(ev.value("label").toInt(0));
        event.intensity = static_cast<float>(ev.value("intensity").toDouble(0.0));
        event.confidence = static_cast<float>(ev.value("confidence").toDouble(0.0));
        events_.push_back(event);
    }
}

} // namespace ArtifactCore
