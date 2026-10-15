#pragma once

#include "MaterialTransfer.hpp"

namespace ArtifactCore::Experimental::MaterialTransfer {

struct DissolveSample {
    Sample source;
    // Absolute simulation tick, resolved from direction/noise on the cold path.
    std::int64_t releaseTick = 0;
};

inline bool sourceVisible(const DissolveSample& sample, std::int64_t tick) noexcept {
    return tick < sample.releaseTick;
}

// Interval is (previousTick, currentTick]. Adjacent forward steps do not overlap.
// Reverse seek requires restoring/replaying solver state, not negative emission.
// A deferred interval must be retried unchanged before advancing the owner clock.
// Samples are strictly ID-sorted; output and source storage must not overlap.
// Visibility changes are published only after the matching spawn batch commits.
inline Result prepareDissolve(const Request& request,
                              std::span<const DissolveSample> samples,
                              std::int64_t previousTick,
                              std::span<Spawn> output) noexcept {
    // Reuse request/space validation without writing any output.
    const auto validation = prepare(request, {}, {});
    if (validation.status != Status::Ready) return validation;
    if (request.tick < previousTick || request.mode != Mode::Convert) return {};
    std::size_t count = 0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        if (!valid(sample.source) ||
            (i != 0 && samples[i - 1].source.id >= sample.source.id)) return {};
        if (previousTick < sample.releaseTick && sample.releaseTick <= request.tick)
            ++count;
    }
    if (count > output.size()) return {Status::Defer};
    std::size_t index = 0;
    for (const auto& sample : samples) {
        if (previousTick < sample.releaseTick && sample.releaseTick <= request.tick) {
            output[index++] = {request.connectionId, request.occurrenceId,
                               sample.releaseTick, sample.source};
        }
    }
    return {Status::Ready, count, count != 0};
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
