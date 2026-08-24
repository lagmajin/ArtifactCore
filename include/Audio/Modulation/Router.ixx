module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
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

    struct ModulationAssignment {
        std::uint32_t sourceId{0};
        std::uint64_t targetId{0};
        float depth{1.0f};
        bool enabled{true};
    };

    class ModulationRouter {
    public:
        std::uint32_t addSource(std::unique_ptr<IModulatorSource> source) {
            if (!source) {
                return 0;
            }
            source->setSampleRate(sampleRate_);
            const std::uint32_t id = nextSourceId_++;
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

        void addAssignment(const ModulationAssignment& assignment) {
            if (assignment.sourceId == 0 || assignment.targetId == 0) {
                return;
            }
            AssignmentRecord record;
            record.sourceId = assignment.sourceId;
            record.targetId = assignment.targetId;
            record.depth = std::isfinite(assignment.depth) ? assignment.depth : 0.0f;
            record.enabled = assignment.enabled;
            assignments_.push_back(std::move(record));
            markDirty();
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
        }

        void process(std::uint32_t numFrames) {
            resolveIfDirty();

            sourceValues_.resize(sources_.size());
            for (std::size_t i = 0; i < sources_.size(); ++i) {
                sourceValues_[i] = sources_[i].source
                    ? sources_[i].source->process(numFrames) : 0.0f;
            }

            std::fill(targetSums_.begin(), targetSums_.end(), 0.0f);

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
                targetSums_[static_cast<std::size_t>(record.targetIndex)] +=
                    record.smoothed;
            }
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

        float targetValue(std::uint64_t targetId, float baseValue) const {
            return baseValue + targetModulation(targetId);
        }

    private:
        struct SourceEntry {
            std::uint32_t id{0};
            std::unique_ptr<IModulatorSource> source;
        };

        struct AssignmentRecord {
            std::uint32_t sourceId{0};
            std::uint64_t targetId{0};
            float depth{1.0f};
            bool enabled{true};
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
        std::vector<float> sourceValues_;
        float sampleRate_{48000.0f};
        float smoothingTime_{0.02f};
        std::uint32_t nextSourceId_{1};
        bool dirty_{false};
    };

} // namespace Modulation
} // namespace Audio
} // namespace ArtifactCore
