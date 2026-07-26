module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

export module Sequence.SequenceGroup;

export namespace ArtifactCore {

enum class SequenceOrder {
    Forward,
    Reverse,
    CenterOut,
    OutsideIn,
};

enum class SequenceDistribution {
    Simultaneous,
    Stagger,
};

enum class SequencePhaseStart {
    AfterPreviousPhase,
    Absolute,
    ElementStart,
    PreviousPhaseComplete,
};

struct SequencePhase {
    std::string name;
    double duration = 0.0;
    double startOffset = 0.0;
    SequencePhaseStart start = SequencePhaseStart::AfterPreviousPhase;
    std::string easing = "linear";
};

struct SequenceTiming {
    double startDelay = 0.0;
    double elementInterval = 0.0;
    double playbackRate = 1.0;
    bool reverse = false;
};

struct SequenceTargetSelector {
    std::string scope = "children";
    std::string filter;
    std::vector<std::string> explicitIds;
};

struct SequenceOverrides {
    bool hasElementInterval = false;
    double elementInterval = 0.0;
    bool hasPlaybackRate = false;
    double playbackRate = 1.0;
    bool hasReverse = false;
    bool reverse = false;
};

struct SequenceDefinition {
    std::string name;
    SequenceTargetSelector target;
    SequenceOrder order = SequenceOrder::Forward;
    SequenceDistribution distribution = SequenceDistribution::Stagger;
    SequenceTiming timing;
    std::vector<SequencePhase> phases;

    double elementStartTime(std::int32_t elementIndex, std::int32_t elementCount) const {
        if (elementCount <= 1 || distribution == SequenceDistribution::Simultaneous) {
            return timing.startDelay;
        }

        const auto orderIndex = orderedIndex(elementIndex, elementCount);
        return timing.startDelay + static_cast<double>(orderIndex) * timing.elementInterval;
    }

    double duration() const {
        double phaseDuration = 0.0;
        for (const auto& phase : phases) {
            phaseDuration += std::max(0.0, phase.duration);
        }
        return timing.startDelay + phaseDuration;
    }

    double durationForElementCount(std::int32_t elementCount) const {
        if (elementCount <= 0) {
            return 0.0;
        }
        return elementStartTime(elementCount - 1, elementCount) + phaseDuration();
    }

    double phaseStartTime(std::int32_t elementIndex, std::int32_t elementCount,
                          std::int32_t phaseIndex) const {
        if (phaseIndex < 0 || phaseIndex >= static_cast<std::int32_t>(phases.size())) {
            return 0.0;
        }

        const auto& phase = phases[static_cast<std::size_t>(phaseIndex)];
        if (phase.start == SequencePhaseStart::Absolute) {
            return phase.startOffset;
        }
        if (phase.start == SequencePhaseStart::ElementStart ||
            phase.start == SequencePhaseStart::PreviousPhaseComplete) {
            return elementStartTime(elementIndex, elementCount) + phase.startOffset +
                   precedingPhaseDuration(phaseIndex);
        }
        return elementStartTime(elementIndex, elementCount) + phase.startOffset +
               precedingPhaseDuration(phaseIndex);
    }

    bool isValid() const {
        return !phases.empty() && timing.elementInterval >= 0.0 && timing.startDelay >= 0.0 &&
               timing.playbackRate > 0.0 && std::all_of(phases.begin(), phases.end(), [](const auto& phase) {
                   return phase.duration >= 0.0;
               });
    }

private:
    double phaseDuration() const {
        double result = 0.0;
        for (const auto& phase : phases) {
            result += std::max(0.0, phase.duration);
        }
        return result;
    }

    double precedingPhaseDuration(std::int32_t phaseIndex) const {
        double result = 0.0;
        for (std::int32_t index = 0; index < phaseIndex; ++index) {
            result += std::max(0.0, phases[static_cast<std::size_t>(index)].duration);
        }
        return result;
    }

    std::int32_t orderedIndex(std::int32_t elementIndex, std::int32_t elementCount) const {
        elementIndex = std::clamp(elementIndex, 0, elementCount - 1);
        switch (order) {
        case SequenceOrder::Reverse:
            return elementCount - 1 - elementIndex;
        case SequenceOrder::CenterOut: {
            const auto center = (elementCount - 1) / 2;
            const auto distance = std::abs(elementIndex - center);
            return distance * 2 + (elementIndex < center ? 1 : 0);
        }
        case SequenceOrder::OutsideIn:
            return std::min(elementIndex, elementCount - 1 - elementIndex) * 2 +
                   (elementIndex > (elementCount - 1) / 2 ? 1 : 0);
        case SequenceOrder::Forward:
        default:
            return elementIndex;
        }
    }
};

enum class SequencePlaybackState {
    Stopped,
    Playing,
    Paused,
    Completed,
};

struct SequencePlayback {
    SequencePlaybackState state = SequencePlaybackState::Stopped;
    double time = 0.0;
    bool reverse = false;
    std::int32_t elementCount = 0;
};

// Runtime-only transport for a SequenceDefinition. It does not mutate layer
// properties; the host evaluates the current time and applies the result.
class SequencePlayer {
public:
    explicit SequencePlayer(const SequenceDefinition* definition = nullptr)
        : definition_(definition) {}

    void setDefinition(const SequenceDefinition* definition) {
        definition_ = definition;
        reset();
    }

    const SequenceDefinition* definition() const { return definition_; }

    void setElementCount(std::int32_t count) {
        playback_.elementCount = std::max<std::int32_t>(0, count);
        clampTime();
    }

    void setOverrides(const SequenceOverrides& overrides) {
        overrides_ = overrides;
        clampTime();
    }

    const SequenceOverrides& overrides() const { return overrides_; }

    std::int32_t elementCount() const { return playback_.elementCount; }

    void play(bool reverse = false) {
        playback_.reverse = overrides_.hasReverse ? overrides_.reverse : reverse;
        if (reverse && playback_.time <= 0.0 && definition_) {
            playback_.time = definition_->durationForElementCount(playback_.elementCount);
        }
        playback_.state = SequencePlaybackState::Playing;
    }

    void pause() { playback_.state = SequencePlaybackState::Paused; }

    void stop() {
        playback_.state = SequencePlaybackState::Stopped;
        playback_.time = 0.0;
    }

    void reset() {
        playback_ = {};
        playback_.state = SequencePlaybackState::Stopped;
    }

    void reverse() {
        playback_.reverse = !playback_.reverse;
        if (playback_.reverse && playback_.time <= 0.0 && definition_) {
            playback_.time = definition_->durationForElementCount(playback_.elementCount);
        }
        playback_.state = SequencePlaybackState::Playing;
    }

    void advance(double deltaSeconds) {
        if (playback_.state != SequencePlaybackState::Playing || !definition_ || deltaSeconds <= 0.0) {
            return;
        }

        const auto total = definition_->durationForElementCount(playback_.elementCount);
        const auto direction = playback_.reverse ? -1.0 : 1.0;
        playback_.time += deltaSeconds * direction * playbackRate();
        if (playback_.time >= total) {
            playback_.time = total;
            playback_.state = SequencePlaybackState::Completed;
        } else if (playback_.time <= 0.0) {
            playback_.time = 0.0;
            playback_.state = SequencePlaybackState::Completed;
        }
    }

    const SequencePlayback& playback() const { return playback_; }
    double time() const { return playback_.time; }

    double phaseStartTime(std::int32_t elementIndex, std::int32_t phaseIndex) const {
        if (!definition_) {
            return 0.0;
        }
        const auto base = definition_->phaseStartTime(elementIndex, playback_.elementCount, phaseIndex);
        if (!overrides_.hasElementInterval || definition_->distribution != SequenceDistribution::Stagger) {
            return base;
        }
        const auto baseStart = definition_->elementStartTime(elementIndex, playback_.elementCount);
        const auto overrideStart = definition_->timing.startDelay +
            static_cast<double>(orderedIndex(elementIndex)) * overrides_.elementInterval;
        return base + overrideStart - baseStart;
    }

private:
    double playbackRate() const {
        if (overrides_.hasPlaybackRate) {
            return std::max(0.001, overrides_.playbackRate);
        }
        return std::max(0.001, definition_->timing.playbackRate);
    }

    std::int32_t orderedIndex(std::int32_t elementIndex) const {
        if (!definition_ || playback_.elementCount <= 1) {
            return 0;
        }
        switch (definition_->order) {
        case SequenceOrder::Reverse:
            return playback_.elementCount - 1 - elementIndex;
        default:
            return std::clamp(elementIndex, 0, playback_.elementCount - 1);
        }
    }

    void clampTime() {
        if (!definition_) {
            playback_.time = 0.0;
            return;
        }
        playback_.time = std::clamp(playback_.time, 0.0,
                                    definition_->durationForElementCount(playback_.elementCount));
    }

    const SequenceDefinition* definition_ = nullptr;
    SequencePlayback playback_;
    SequenceOverrides overrides_;
};

}
