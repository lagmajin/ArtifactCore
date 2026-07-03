module;
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

module Audio.LipSyncTrack;

import Audio.Analyze;
import Audio.FormantExtractor;

namespace ArtifactCore {

LipSyncTrack::LipSyncTrack() = default;
LipSyncTrack::~LipSyncTrack() = default;

bool LipSyncTrack::analyze(const AudioSegment& segment, double frameRate)
{
    events_ = extractor_.analyzeTrack(segment, frameRate);
    return !events_.empty();
}

bool LipSyncTrack::analyzeFromFile(const QString& audioPath, double frameRate)
{
    // AudioSegment をファイルから構築（簡易実装）
    // 本実装では外部オーディオファイルの読み込みは別途行う前提
    // ここでは空を返す（実使用では analyze() を直接呼ぶ）
    return false;
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
