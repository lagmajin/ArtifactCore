module;

#include <chrono>
#include <cstdint>
#include <QMutex>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Playback.Clock;

import std;
import Frame.Rate;
import Frame.Position;

export namespace ArtifactCore {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

// 高精度再生クロック
// 注意: UI更新にはSignal/Slotを使わず、定期ポーリングを推奨
// Signal/Slotはキューイング遅延により高精度が失われるため
class LIBRARY_DLL_API PlaybackClock {
public:
    class Impl;
    Impl* impl_ = nullptr;

    PlaybackClock();
    explicit PlaybackClock(const FrameRate& frameRate);
    PlaybackClock(const PlaybackClock& other);
    PlaybackClock(PlaybackClock&& other) noexcept;
    ~PlaybackClock();

    PlaybackClock& operator=(const PlaybackClock& other);
    PlaybackClock& operator=(PlaybackClock&& other) noexcept;

    // 基本制御
    void start();
    void pause();
    void stop();
    void resume();

    // 状態取得
    PlaybackState state() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    // 時間取得
    std::chrono::microseconds elapsedTime() const;  // マイクロ秒精度
    std::chrono::milliseconds elapsedTimeMs() const;
    double elapsedSeconds() const;

    // フレーム位置
    std::int64_t currentFrame() const;
    FramePosition currentPosition() const;
    void setFrame(std::int64_t frame);
    void setPosition(const FramePosition& position);

    // フレームレート
    void setFrameRate(const FrameRate& frameRate);
    FrameRate frameRate() const;
    double framesPerSecond() const;

    // 再生速度制御
    void setPlaybackSpeed(double speed);  // 1.0 = 通常, 0.5 = 半速, 2.0 = 倍速, -1.0 = 逆再生
    double playbackSpeed() const;
    bool isReversePlaying() const;

    // ループ制御
    void setLoopRange(std::int64_t startFrame, std::int64_t endFrame);
    void clearLoopRange();
    bool isLooping() const;
    std::int64_t loopStartFrame() const;
    std::int64_t loopEndFrame() const;

    // オーディオ同期
    void syncToAudioClock(std::chrono::microseconds audioTime);
    void setAudioSyncEnabled(bool enabled);
    bool isAudioSyncEnabled() const;
    std::chrono::microseconds audioOffset() const;

    // ドロップフレーム検出
    void setDropFrameDetectionEnabled(bool enabled);
    bool isDropFrameDetectionEnabled() const;
    std::int64_t droppedFrameCount() const;
    void resetDroppedFrameCount();

    // タイムコード
    QString timecode() const;  // HH:MM:SS:FF
    QString timecodeWithSubframe() const;  // HH:MM:SS:FF.sf

    // デルタタイム（前回の更新からの経過時間）
    std::chrono::microseconds deltaTime();

    // リセット
    void reset();

    // デバッグ
    QString statistics() const;
};

}
