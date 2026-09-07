module;
#include <cstddef>
#include <cstdint>
#include "../Define/DllExportMacro.hpp"

export module Graphics.GPUAlgorithms;

export namespace ArtifactCore {

enum class GPUAlgorithmStatus : std::uint8_t { Valid, EmptyInput, InvalidInput, Overflow };

struct GPUDispatchPlan {
    GPUAlgorithmStatus status = GPUAlgorithmStatus::InvalidInput;
    std::size_t elementCount = 0;
    std::size_t elementStrideBytes = 0;
    std::size_t workgroupSize = 256;
    std::size_t workgroupCount = 0;
    std::size_t temporaryElementCount = 0;
    bool requiresIndirectDispatch = false;
};

struct GPUAlgorithmLimits {
    std::size_t maxElements = 1u << 28;
    std::size_t maxWorkgroupSize = 1024;
    std::size_t radixBits = 4;
};

class LIBRARY_DLL_API GPUAlgorithms {
public:
    static GPUDispatchPlan prefixSum(std::size_t elementCount,
                                     std::size_t elementStrideBytes = sizeof(std::uint32_t),
                                     GPUAlgorithmLimits limits = {});
    static GPUDispatchPlan compaction(std::size_t elementCount,
                                      std::size_t elementStrideBytes,
                                      GPUAlgorithmLimits limits = {});
    static GPUDispatchPlan radixSort(std::size_t elementCount,
                                     std::size_t keyStrideBytes = sizeof(std::uint32_t),
                                     GPUAlgorithmLimits limits = {});
};

}
