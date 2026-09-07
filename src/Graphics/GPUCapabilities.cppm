module;
#include <cstdint>
#include <limits>
#include "../../include/Define/DllExportMacro.hpp"

module Graphics.GPUCapabilities;

namespace ArtifactCore {

GPUCapabilitySnapshot GPUCapabilities::normalize(const GPUCapabilitySnapshot& source) {
    auto result = source;
    if (!result.memoryBudget) result.memory = {};
    if (result.memory.valid && result.memory.budgetBytes == 0) result.memory.valid = false;
    if (result.memory.valid && result.memory.usageBytes > result.memory.budgetBytes)
        result.memory.usageBytes = result.memory.budgetBytes;
    return result;
}

GPUTimelineToken GPUCapabilities::nextTimelineToken(std::uint64_t currentValue) noexcept {
    if (currentValue == std::numeric_limits<std::uint64_t>::max()) return {};
    return {currentValue + 1, true};
}

GPUIndirectCountPlan GPUCapabilities::indirectCount(std::size_t maxCommandCount,
                                                    std::size_t commandStrideBytes,
                                                    const GPUCapabilitySnapshot& snapshot) noexcept {
    return {maxCommandCount, commandStrideBytes,
            snapshot.indirectCount && maxCommandCount > 0 && commandStrideBytes > 0};
}

bool GPUCapabilities::hasUsableMemoryBudget(const GPUCapabilitySnapshot& snapshot) noexcept {
    return snapshot.memoryBudget && snapshot.memory.valid &&
           snapshot.memory.budgetBytes > 0 &&
           snapshot.memory.usageBytes <= snapshot.memory.budgetBytes;
}

}
