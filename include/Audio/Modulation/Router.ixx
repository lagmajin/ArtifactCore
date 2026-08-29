module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import Audio.Modulation.Modulator;

export module Audio.Modulation.Router;

namespace ArtifactCore {
namespace Audio {
namespace Modulation {
namespace Detail {

    inline float sanitizedSampleRate(float sampleRate) {
        return (std::isfinite(sampleRate) && sampleRate > 0.0f) ? sampleRate : 48000.0f;
    }

} // namespace Detail
} // namespace Modulation
} // namespace Audio
} // namespace ArtifactCore

export namespace ArtifactCore {
namespace Audio {
namespace Modulation {

    constexpr std::uint64_t modulationTargetId(std::string_view path) noexcept {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const char c : path) {
            hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    enum class ModulationMixMode : std::uint8_t {
        Add,
        Multiply
    };

    enum class ModulatorSourceType : std::uint8_t {
        Lfo,
        Adsr,
        Random,
        Macro
    };

    // Configuration only: runtime phase, held samples, and ADSR gate state
    // are reset after restore so a project reopen starts deterministically.
    struct ModulationSourceDefinition {
        std::uint32_t id{0};
        ModulatorSourceType type{ModulatorSourceType::Lfo};
        LfoWaveform waveform{LfoWaveform::Sine};
        float frequency{1.0f};
        float phaseOffset{0.0f};
        float pulseWidth{0.5f};
        float attack{0.01f};
        float decay{0.1f};
        float sustain{0.7f};
        float release{0.2f};
        float rate{1.0f};
        float smoothing{0.005f};
        std::uint32_t seed{2463534242u};
        float macroValue{0.0f};
        bool unipolar{false};
    };

    // A stable, serializable description of a modulation mapping. Property
    // paths are retained for callers that persist mappings; runtime lookup is
    // performed through targetId to avoid string work in process().
    struct ModulationAssignment {
        std::uint32_t sourceId{0};
        std::uint64_t targetId{0};
        std::string targetPath;
        float depth{1.0f};
        bool enabled{true};
        ModulationMixMode mode{ModulationMixMode::Add};

        static ModulationAssignment forPropertyPath(
            std::uint32_t sourceId, std::string_view targetPath,
            float depth = 1.0f,
            ModulationMixMode mode = ModulationMixMode::Add) {
            ModulationAssignment assignment;
            assignment.sourceId = sourceId;
            assignment.targetPath = std::string(targetPath);
            assignment.targetId = modulationTargetId(targetPath);
            assignment.depth = depth;
            assignment.mode = mode;
            return assignment;
        }
    };

    struct ModulationRouterSnapshot {
        std::vector<ModulationSourceDefinition> sources;
        std::vector<ModulationAssignment> assignments;
        float smoothingTime{0.02f};
    };

    class ModulationRouter {
    public:
        std::uint32_t addSource(std::unique_ptr<IModulatorSource> source) {
            if (!source) {
                return 0;
            }
            source->setSampleRate(sampleRate_);
            std::uint32_t id = nextSourceId_ == 0 ? 1u : nextSourceId_;
            const std::uint32_t firstCandidate = id;
            while (findSource(id)) {
                ++id;
                if (id == 0) id = 1;
                if (id == firstCandidate) return 0;
            }
            nextSourceId_ = id + 1u;
            if (nextSourceId_ == 0) nextSourceId_ = 1u;
            sources_.push_back(SourceEntry{id, std::move(source)});
            markDirty();
            return id;
        }

        bool removeSource(std::uint32_t sourceId) {
            const auto it = std::find_if(sources_.begin(), sources_.end(),
                [sourceId](const SourceEntry& entry) { return entry.id == sourceId; });
            if (it == sources_.end()) {
                return false;
            }
            sources_.erase(it);
            std::erase_if(assignments_, [sourceId](const AssignmentRecord& record) {
                return record.sourceId == sourceId;
            });
            markDirty();
            return true;
        }

        IModulatorSource* source(std::uint32_t sourceId) {
            SourceEntry* entry = findSource(sourceId);
            return entry ? entry->source.get() : nullptr;
        }

        bool addAssignment(const ModulationAssignment& assignment) {
            if (assignment.sourceId == 0 || assignment.targetId == 0 ||
                !findSource(assignment.sourceId)) {
                return false;
            }
            const float depth = std::isfinite(assignment.depth) ? assignment.depth : 0.0f;
            const auto existing = std::find_if(assignments_.begin(), assignments_.end(),
                [&](const AssignmentRecord& record) {
                    return record.sourceId == assignment.sourceId &&
                           record.targetId == assignment.targetId &&
                           record.mode == assignment.mode;
                });
            if (existing != assignments_.end()) {
                existing->targetPath = assignment.targetPath;
                existing->depth = depth;
                existing->enabled = assignment.enabled;
                markDirty();
                return true;
            }
            AssignmentRecord record;
            record.sourceId = assignment.sourceId;
            record.targetId = assignment.targetId;
            record.targetPath = assignment.targetPath;
            record.depth = depth;
            record.enabled = assignment.enabled;
            record.mode = assignment.mode;
            assignments_.push_back(std::move(record));
            markDirty();
            return true;
        }

        std::vector<ModulationAssignment> assignments() const {
            std::vector<ModulationAssignment> result;
            result.reserve(assignments_.size());
            for (const auto& record : assignments_) {
                result.push_back({record.sourceId, record.targetId, record.targetPath,
                                  record.depth, record.enabled, record.mode});
            }
            return result;
        }

        std::vector<ModulationSourceDefinition> sourceDefinitions() const {
            std::vector<ModulationSourceDefinition> result;
            result.reserve(sources_.size());
            for (const auto& entry : sources_) {
                if (!entry.source) {
                    continue;
                }
                ModulationSourceDefinition definition;
                definition.id = entry.id;
                if (const auto* lfo = dynamic_cast<const LfoSource*>(entry.source.get())) {
                    definition.type = ModulatorSourceType::Lfo;
                    definition.waveform = lfo->waveform();
                    definition.frequency = lfo->frequency();
                    definition.phaseOffset = lfo->phaseOffset();
                    definition.pulseWidth = lfo->pulseWidth();
                    definition.unipolar = lfo->unipolar();
                } else if (const auto* adsr = dynamic_cast<const AdsrSource*>(entry.source.get())) {
                    definition.type = ModulatorSourceType::Adsr;
                    definition.attack = adsr->attack();
                    definition.decay = adsr->decay();
                    definition.sustain = adsr->sustain();
                    definition.release = adsr->release();
                } else if (const auto* random = dynamic_cast<const RandomSource*>(entry.source.get())) {
                    definition.type = ModulatorSourceType::Random;
                    definition.rate = random->rate();
                    definition.smoothing = random->smoothing();
                    definition.seed = random->seed();
                    definition.unipolar = random->unipolar();
                } else if (const auto* macro = dynamic_cast<const MacroSource*>(entry.source.get())) {
                    definition.type = ModulatorSourceType::Macro;
                    definition.macroValue = macro->value();
                } else {
                    continue;
                }
                result.push_back(definition);
            }
            return result;
        }

        ModulationRouterSnapshot snapshot() const {
            return {sourceDefinitions(), assignments(), smoothingTime_};
        }

        void restoreSnapshot(const ModulationRouterSnapshot& snapshot) {
            assignments_.clear();
            setSmoothingTime(snapshot.smoothingTime);
            restoreSources(snapshot.sources);
            for (const auto& assignment : snapshot.assignments) {
                addAssignment(assignment);
            }
            reset();
        }

        void restoreSources(const std::vector<ModulationSourceDefinition>& definitions) {
            sources_.clear();
            std::uint32_t largestId = 0;
            for (const auto& definition : definitions) {
                if (definition.id == 0 || findSource(definition.id)) {
                    continue;
                }
                std::unique_ptr<IModulatorSource> source;
                switch (definition.type) {
                    case ModulatorSourceType::Lfo: {
                        auto lfo = std::make_unique<LfoSource>(definition.waveform);
                        lfo->setFrequency(definition.frequency);
                        lfo->setPhase(definition.phaseOffset);
                        lfo->setPulseWidth(definition.pulseWidth);
                        lfo->setUnipolar(definition.unipolar);
                        source = std::move(lfo);
                        break;
                    }
                    case ModulatorSourceType::Adsr: {
                        auto adsr = std::make_unique<AdsrSource>();
                        adsr->setAttack(definition.attack);
                        adsr->setDecay(definition.decay);
                        adsr->setSustain(definition.sustain);
                        adsr->setRelease(definition.release);
                        source = std::move(adsr);
                        break;
                    }
                    case ModulatorSourceType::Random: {
                        auto random = std::make_unique<RandomSource>();
                        random->setRate(definition.rate);
                        random->setSmoothing(definition.smoothing);
                        random->setSeed(definition.seed);
                        random->setUnipolar(definition.unipolar);
                        source = std::move(random);
                        break;
                    }
                    case ModulatorSourceType::Macro: {
                        auto macro = std::make_unique<MacroSource>();
                        macro->setValue(definition.macroValue);
                        source = std::move(macro);
                        break;
                    }
                }
                if (!source) {
                    continue;
                }
                source->setSampleRate(sampleRate_);
                sources_.push_back(SourceEntry{definition.id, std::move(source)});
                largestId = std::max(largestId, definition.id);
            }
            nextSourceId_ = largestId == std::numeric_limits<std::uint32_t>::max()
                ? 1u : largestId + 1u;
            std::erase_if(assignments_, [this](const AssignmentRecord& record) {
                return !findSource(record.sourceId);
            });
            markDirty();
            reset();
        }

        void removeAssignment(std::uint32_t sourceId, std::uint64_t targetId) {
            std::erase_if(assignments_, [sourceId, targetId](const AssignmentRecord& record) {
                return record.sourceId == sourceId && record.targetId == targetId;
            });
            markDirty();
        }

        void removeAssignmentsForSource(std::uint32_t sourceId) {
            std::erase_if(assignments_, [sourceId](const AssignmentRecord& record) {
                return record.sourceId == sourceId;
            });
            markDirty();
        }

        void removeAssignmentsForTarget(std::uint64_t targetId) {
            std::erase_if(assignments_, [targetId](const AssignmentRecord& record) {
                return record.targetId == targetId;
            });
            markDirty();
        }

        bool setDepth(std::uint32_t sourceId, std::uint64_t targetId, float depth) {
            bool changed = false;
            for (auto& record : assignments_) {
                if (record.sourceId == sourceId && record.targetId == targetId) {
                    record.depth = std::isfinite(depth) ? depth : 0.0f;
                    changed = true;
                }
            }
            return changed;
        }

        bool setEnabled(std::uint32_t sourceId, std::uint64_t targetId, bool enabled) {
            bool changed = false;
            for (auto& record : assignments_) {
                if (record.sourceId == sourceId && record.targetId == targetId) {
                    record.enabled = enabled;
                    changed = true;
                }
            }
            return changed;
        }

        void clearAssignments() {
            assignments_.clear();
            markDirty();
        }

        void setSampleRate(float sampleRate) {
            sampleRate_ = Detail::sanitizedSampleRate(sampleRate);
            for (auto& entry : sources_) {
                if (entry.source) {
                    entry.source->setSampleRate(sampleRate_);
                }
            }
        }

        void setSmoothingTime(float seconds) {
            smoothingTime_ = std::isfinite(seconds) && seconds > 0.0f
                ? seconds : 0.0f;
        }

        float smoothingTime() const { return smoothingTime_; }

        void reset() {
            for (auto& entry : sources_) {
                if (entry.source) {
                    entry.source->reset();
                }
            }
            for (auto& record : assignments_) {
                record.smoothed = 0.0f;
            }
            resolveIfDirty();
            std::fill(targetSums_.begin(), targetSums_.end(), 0.0f);
            std::fill(targetMultipliers_.begin(), targetMultipliers_.end(), 1.0f);
            lastControlFrame_.reset();
            controlFrameRate_.reset();
        }

        void process(std::uint32_t numFrames) {
            resolveIfDirty();

            sourceValues_.resize(sources_.size());
            for (std::size_t i = 0; i < sources_.size(); ++i) {
                sourceValues_[i] = sources_[i].source
                    ? sources_[i].source->process(numFrames) : 0.0f;
            }

            std::fill(targetSums_.begin(), targetSums_.end(), 0.0f);
            std::fill(targetMultipliers_.begin(), targetMultipliers_.end(), 1.0f);

            float coefficient = 1.0f;
            if (smoothingTime_ > 0.0f && numFrames > 0) {
                const float deltaSeconds =
                    static_cast<float>(numFrames) / sampleRate_;
                coefficient = 1.0f - std::exp(-deltaSeconds / smoothingTime_);
                if (!std::isfinite(coefficient)) {
                    coefficient = 1.0f;
                }
            }

            for (auto& record : assignments_) {
                if (!record.enabled ||
                    record.sourceIndex < 0 || record.targetIndex < 0) {
                    continue;
                }
                const float goal =
                    sourceValues_[static_cast<std::size_t>(record.sourceIndex)] *
                    record.depth;
                record.smoothed += (goal - record.smoothed) * coefficient;
                const std::size_t targetIndex = static_cast<std::size_t>(record.targetIndex);
                if (record.mode == ModulationMixMode::Add) {
                    targetSums_[targetIndex] += record.smoothed;
                } else {
                    const float factor = 1.0f + record.smoothed;
                    if (std::isfinite(factor)) {
                        targetMultipliers_[targetIndex] *= factor;
                    }
                }
            }
        }

        // Deterministic control-rate evaluation for timeline-owned callers.
        // The same frame is idempotent; seeking backward resets sources and
        // replays from frame zero. Audio callers should keep using process().
        void processAtFrame(std::int64_t frame, float frameRate) {
            const std::int64_t safeFrame = std::max<std::int64_t>(0, frame);
            const float safeFrameRate = Detail::sanitizedSampleRate(frameRate);
            const bool rateChanged = !controlFrameRate_.has_value() ||
                std::abs(*controlFrameRate_ - safeFrameRate) > 0.0001f;
            setSampleRate(safeFrameRate);

            if (!lastControlFrame_.has_value() || rateChanged ||
                safeFrame < *lastControlFrame_) {
                reset();
                controlFrameRate_ = safeFrameRate;
                lastControlFrame_ = 0;
            }

            std::int64_t remaining = safeFrame - *lastControlFrame_;
            while (remaining > 0) {
                const auto chunk = static_cast<std::uint32_t>(std::min<std::int64_t>(
                    remaining, std::numeric_limits<std::uint32_t>::max()));
                process(chunk);
                remaining -= static_cast<std::int64_t>(chunk);
            }
            if (safeFrame == 0) {
                process(0);
            }
            lastControlFrame_ = safeFrame;
        }

        float targetModulation(std::uint64_t targetId) const {
            const auto it = std::lower_bound(
                targetIds_.begin(), targetIds_.end(), targetId);
            if (it == targetIds_.end() || *it != targetId) {
                return 0.0f;
            }
            return targetSums_[static_cast<std::size_t>(
                std::distance(targetIds_.begin(), it))];
        }

        bool hasTarget(std::uint64_t targetId) const {
            const auto it = std::lower_bound(targetIds_.begin(), targetIds_.end(), targetId);
            return it != targetIds_.end() && *it == targetId;
        }

        float targetValue(std::uint64_t targetId, float baseValue) const {
            const auto it = std::lower_bound(targetIds_.begin(), targetIds_.end(), targetId);
            if (it == targetIds_.end() || *it != targetId) {
                return baseValue;
            }
            const std::size_t index = static_cast<std::size_t>(std::distance(targetIds_.begin(), it));
            return (baseValue + targetSums_[index]) * targetMultipliers_[index];
        }

    private:
        struct SourceEntry {
            std::uint32_t id{0};
            std::unique_ptr<IModulatorSource> source;
        };

        struct AssignmentRecord {
            std::uint32_t sourceId{0};
            std::uint64_t targetId{0};
            std::string targetPath;
            float depth{1.0f};
            bool enabled{true};
            ModulationMixMode mode{ModulationMixMode::Add};
            int sourceIndex{-1};
            int targetIndex{-1};
            float smoothed{0.0f};
        };

        SourceEntry* findSource(std::uint32_t sourceId) {
            const auto it = std::find_if(sources_.begin(), sources_.end(),
                [sourceId](const SourceEntry& entry) { return entry.id == sourceId; });
            return it != sources_.end() ? &(*it) : nullptr;
        }

        void markDirty() {
            dirty_ = true;
        }

        void rebuildTargets() {
            targetIds_.clear();
            targetIds_.reserve(assignments_.size());
            for (const auto& record : assignments_) {
                targetIds_.push_back(record.targetId);
            }
            std::sort(targetIds_.begin(), targetIds_.end());
            targetIds_.erase(
                std::unique(targetIds_.begin(), targetIds_.end()),
                targetIds_.end());
            targetSums_.assign(targetIds_.size(), 0.0f);
            targetMultipliers_.assign(targetIds_.size(), 1.0f);
        }

        void resolveIfDirty() {
            if (!dirty_) {
                return;
            }
            dirty_ = false;

            rebuildTargets();

            for (auto& record : assignments_) {
                record.smoothed = 0.0f;
                record.sourceIndex = -1;
                record.targetIndex = -1;

                for (std::size_t i = 0; i < sources_.size(); ++i) {
                    if (sources_[i].id == record.sourceId) {
                        record.sourceIndex = static_cast<int>(i);
                        break;
                    }
                }

                const auto it = std::lower_bound(
                    targetIds_.begin(), targetIds_.end(), record.targetId);
                if (it != targetIds_.end() && *it == record.targetId) {
                    record.targetIndex = static_cast<int>(
                        std::distance(targetIds_.begin(), it));
                }
            }
        }

        std::vector<SourceEntry> sources_;
        std::vector<AssignmentRecord> assignments_;
        std::vector<std::uint64_t> targetIds_;
        std::vector<float> targetSums_;
        std::vector<float> targetMultipliers_;
        std::vector<float> sourceValues_;
        float sampleRate_{48000.0f};
        float smoothingTime_{0.02f};
        std::uint32_t nextSourceId_{1};
        std::optional<std::int64_t> lastControlFrame_;
        std::optional<float> controlFrameRate_;
        bool dirty_{false};
    };

} // namespace Modulation
} // namespace Audio
} // namespace ArtifactCore
