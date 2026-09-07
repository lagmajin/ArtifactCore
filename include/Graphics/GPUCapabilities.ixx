module;
#include <cstddef>
#include <cstdint>
#include "../Define/DllExportMacro.hpp"

export module Graphics.GPUCapabilities;

export namespace ArtifactCore {

struct GPUMemoryBudget {
    std::uint64_t budgetBytes = 0;
    std::uint64_t usageBytes = 0;
    bool valid = false;
};

struct GPUCapabilitySnapshot {
    bool synchronization2 = false;
    bool timelineSemaphore = false;
    bool memoryBudget = false;
    bool indirectCount = false;
    bool subgroupOperations = false;
    bool descriptorIndexing = false;
    GPUMemoryBudget memory;
};

struct GPUTimelineToken {
    std::uint64_t value = 0;
    bool valid = false;
};

struct GPUIndirectCountPlan {
    std::size_t maxCommandCount = 0;
    std::size_t commandStrideBytes = 0;
    bool supported = false;
};

class LIBRARY_DLL_API GPUCapabilities {
public:
    static GPUCapabilitySnapshot normalize(const GPUCapabilitySnapshot& snapshot);
    static GPUTimelineToken nextTimelineToken(std::uint64_t currentValue) noexcept;
    static GPUIndirectCountPlan indirectCount(std::size_t maxCommandCount,
                                               std::size_t commandStrideBytes,
                                               const GPUCapabilitySnapshot& snapshot) noexcept;
    static bool hasUsableMemoryBudget(const GPUCapabilitySnapshot& snapshot) noexcept;
};

}
