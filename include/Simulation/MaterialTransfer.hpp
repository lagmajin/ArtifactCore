#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

// Experimental, opt-in CPU contract. No solver or application includes this yet.
namespace ArtifactCore::Experimental::MaterialTransfer {

struct Vector3 {
    float x = 0;
    float y = 0;
    float z = 0;
};

enum class Domain : std::uint8_t { Particle, Fragment, Granular };
enum class Mode : std::uint8_t { Convert, SpawnSecondary };

// Both adapters must explicitly agree on units and coordinate space.
// A token is caller-defined; zero means unspecified and is never accepted.
struct Space {
    std::uint64_t token = 0;
    float metersPerUnit = 1;
};

// Fixed standard attributes. Solver-specific state remains with its solver.
struct Sample {
    std::uint64_t id = 0;
    Vector3 position;
    Vector3 velocity;
    float linearRed = 1;
    float linearGreen = 1;
    float linearBlue = 1;
    float alpha = 1;
    double mass = 0;
    double volume = 0;
};

struct Request {
    std::uint64_t connectionId = 0;
    std::uint64_t occurrenceId = 0;
    std::int64_t tick = 0;
    Domain sourceDomain = Domain::Particle;
    Domain targetDomain = Domain::Granular;
    Mode mode = Mode::Convert;
    Space sourceSpace;
    Space targetSpace;
};

struct Spawn {
    // Tuple plus source.id identifies this output; destination allocates its ID.
    std::uint64_t connectionId = 0;
    std::uint64_t occurrenceId = 0;
    std::int64_t tick = 0;
    Sample source;
};

enum class Status : std::uint8_t { Ready, InvalidInput, SpaceMismatch, Defer };

struct Result {
    Status status = Status::InvalidInput;
    std::size_t count = 0;
    // Permission to consume is conditional on destination commit succeeding.
    bool consumeSourceAfterCommit = false;
};

inline bool finite(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

inline bool valid(const Sample& sample) noexcept {
    return sample.id != 0 && finite(sample.position) && finite(sample.velocity) &&
           std::isfinite(sample.linearRed) && sample.linearRed >= 0 &&
           std::isfinite(sample.linearGreen) && sample.linearGreen >= 0 &&
           std::isfinite(sample.linearBlue) && sample.linearBlue >= 0 &&
           std::isfinite(sample.alpha) && sample.alpha >= 0 && sample.alpha <= 1 &&
           std::isfinite(sample.mass) && sample.mass > 0 &&
           std::isfinite(sample.volume) && sample.volume > 0;
}

// One source becomes one destination sample; mass/volume/velocity are preserved.
// Secondary emission supplies its OWN mass/volume budget in the input samples.
// Input and output storage must not overlap. No allocation, logging or transfer.
// Validates the entire batch before writing; failures leave output untouched.
// Source samples must be strictly ID-sorted (resolve ordering on the cold path).
// Caller must deduplicate occurrence tuples and keep them in checkpoints.
// Source removal happens only AFTER the destination atomically commits the batch.
inline Result prepare(const Request& request, std::span<const Sample> input,
                      std::span<Spawn> output) noexcept {
    if (request.connectionId == 0 || request.occurrenceId == 0 ||
        (request.mode != Mode::Convert && request.mode != Mode::SpawnSecondary) ||
        (request.sourceDomain != Domain::Particle &&
         request.sourceDomain != Domain::Fragment &&
         request.sourceDomain != Domain::Granular) ||
        (request.targetDomain != Domain::Particle &&
         request.targetDomain != Domain::Fragment &&
         request.targetDomain != Domain::Granular)) {
        return {};
    }
    if (request.sourceSpace.token == 0 ||
        request.sourceSpace.token != request.targetSpace.token ||
        !std::isfinite(request.sourceSpace.metersPerUnit) ||
        request.sourceSpace.metersPerUnit <= 0 ||
        request.sourceSpace.metersPerUnit != request.targetSpace.metersPerUnit) {
        return {Status::SpaceMismatch};
    }
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (!valid(input[i]) || (i != 0 && input[i - 1].id >= input[i].id)) return {};
    }
    if (output.size() < input.size()) return {Status::Defer};
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = {request.connectionId, request.occurrenceId,
                     request.tick, input[i]};
    }
    return {Status::Ready, input.size(),
            !input.empty() && request.mode == Mode::Convert};
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
