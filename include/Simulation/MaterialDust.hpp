#pragma once

#include "MaterialTransfer.hpp"
#include <limits>

namespace ArtifactCore::Experimental::MaterialTransfer {

struct DustSite {
    // Explicit positions/velocities in the request's agreed coordinate space.
    Vector3 position;
    Vector3 addedVelocity;
};

struct DustBudget {
    // Artistic secondary emission budget, not subtracted from fragment mass.
    // Physical erosion requires a separate conserving conversion transaction.
    double mass = 0;
    double volume = 0;
    float velocityInheritance = 1;
    std::uint64_t firstChildId = 0;
};

struct DustSpawn {
    Spawn particle;
    std::uint64_t parentFragmentId = 0;
};

// One fragment produces one child per site, with an equal share of the budget.
// Caller allocates a globally unique child-ID range before evaluation and retains
// that range across retries. Geometry sampling and random state are external.
// All input/output storage must be disjoint. No source mutation or allocation.
inline Result prepareFragmentDust(const Request& request, const Sample& fragment,
                                  const DustBudget& budget,
                                  std::span<const DustSite> sites,
                                  std::span<DustSpawn> output) noexcept {
    const auto validation = prepare(request, {}, {});
    if (validation.status != Status::Ready) return validation;
    if (request.sourceDomain != Domain::Fragment ||
        request.targetDomain != Domain::Particle ||
        request.mode != Mode::SpawnSecondary || !valid(fragment) ||
        !std::isfinite(budget.mass) || budget.mass <= 0 ||
        !std::isfinite(budget.volume) || budget.volume <= 0 ||
        !std::isfinite(budget.velocityInheritance) || budget.velocityInheritance < 0 ||
        budget.firstChildId == 0) return {};
    if (sites.empty()) return {Status::Ready};
    if (sites.size() - 1 > std::numeric_limits<std::uint64_t>::max() - budget.firstChildId)
        return {};
    const auto lastChildId = budget.firstChildId + sites.size() - 1;
    if (fragment.id >= budget.firstChildId && fragment.id <= lastChildId) return {};
    const double childMass = budget.mass / static_cast<double>(sites.size());
    const double childVolume = budget.volume / static_cast<double>(sites.size());
    if (!std::isfinite(childMass) || childMass <= 0 ||
        !std::isfinite(childVolume) || childVolume <= 0) return {};
    const auto velocity = [&](const DustSite& site) noexcept {
        return Vector3{
            fragment.velocity.x * budget.velocityInheritance + site.addedVelocity.x,
            fragment.velocity.y * budget.velocityInheritance + site.addedVelocity.y,
            fragment.velocity.z * budget.velocityInheritance + site.addedVelocity.z};
    };
    for (const auto& site : sites) {
        if (!finite(site.position) || !finite(site.addedVelocity) || !finite(velocity(site)))
            return {};
    }
    if (output.size() < sites.size()) return {Status::Defer};
    for (std::size_t i = 0; i < sites.size(); ++i) {
        Sample child = fragment;
        child.id = budget.firstChildId + i;
        child.position = sites[i].position;
        child.velocity = velocity(sites[i]);
        child.mass = childMass;
        child.volume = childVolume;
        output[i] = {{request.connectionId, request.occurrenceId, request.tick, child},
                     fragment.id};
    }
    return {Status::Ready, sites.size(), false};
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
