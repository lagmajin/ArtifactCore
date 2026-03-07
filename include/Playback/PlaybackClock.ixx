module;

#include <chrono>
#include <cstdint>
#include <QMutex>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Playback.Clock;

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



import Frame.Rate;
import Frame.Position;

export namespace ArtifactCore {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

// xĐNbN
// : UIXVɂSignal/Slotg킸A|[O𐄏
// Signal/Slot̓L[COxɂ荂x邽
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

    // {
    void start();
    void pause();
    void stop();
    void resume();

    // Ԏ擾
    PlaybackState state() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    // Ԏ擾
    std::chrono::microseconds elapsedTime() const;  // }CNbx
    std::chrono::milliseconds elapsedTimeMs() const;
    double elapsedSeconds() const;

    // t[ʒu
    std::int64_t currentFrame() const;
    FramePosition currentPosition() const;
    void setFrame(std::int64_t frame);
    void setPosition(const FramePosition& position);

    // t[[g
    void setFrameRate(const FrameRate& frameRate);
    FrameRate frameRate() const;
    double framesPerSecond() const;

    // Đx
    void setPlaybackSpeed(double speed);  // 1.0 = ʏ, 0.5 = , 2.0 = {, -1.0 = tĐ
    double playbackSpeed() const;
    bool isReversePlaying() const;

    // [v
    void setLoopRange(std::int64_t startFrame, std::int64_t endFrame);
    void clearLoopRange();
    bool isLooping() const;
    std::int64_t loopStartFrame() const;
    std::int64_t loopEndFrame() const;

    // I[fBI
    void syncToAudioClock(std::chrono::microseconds audioTime);
    void setAudioSyncEnabled(bool enabled);
    bool isAudioSyncEnabled() const;
    std::chrono::microseconds audioOffset() const;

    // hbvt[o
    void setDropFrameDetectionEnabled(bool enabled);
    bool isDropFrameDetectionEnabled() const;
    std::int64_t droppedFrameCount() const;
    void resetDroppedFrameCount();

    // ^CR[h
    QString timecode() const;  // HH:MM:SS:FF
    QString timecodeWithSubframe() const;  // HH:MM:SS:FF.sf

    // f^^CiO̍XV̌oߎԁj
    std::chrono::microseconds deltaTime();

    // Zbg
    void reset();

    // fobO
    QString statistics() const;
};

}
