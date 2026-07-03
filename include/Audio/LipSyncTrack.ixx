module;
#include "../Define/DllExportMacro.hpp"
#include <vector>
#include <cstdint>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

export module Audio.LipSyncTrack;

import Audio.Segment;
import Audio.FormantExtractor;

export namespace ArtifactCore {

/// リップシンクトラック: 音素イベントを管理しSwitch Layerと連携する
class LIBRARY_DLL_API LipSyncTrack {
public:
    LipSyncTrack();
    ~LipSyncTrack();

    /// 解析実行
    bool analyze(const AudioSegment& segment, double frameRate);
    bool analyzeFromFile(const QString& audioPath, double frameRate);

    /// 音素イベント一覧
    const std::vector<PhonemeEvent>& events() const { return events_; }
    std::vector<PhonemeEvent> eventsInRange(int64_t startFrame, int64_t endFrame) const;
    PhonemeEvent eventAtFrame(int64_t frame) const;

    /// 口形状インデックス列（Switch Layer 連携用）
    std::vector<int> mouthShapeSequence() const;

    /// 設定
    void setMouthGroupId(int id) { mouthGroupId_ = id; }
    int mouthGroupId() const { return mouthGroupId_; }

    /// JSON 保存/復元
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

private:
    std::vector<PhonemeEvent> events_;
    int mouthGroupId_ = 0;
    FormantExtractor extractor_;
};

} // namespace ArtifactCore
