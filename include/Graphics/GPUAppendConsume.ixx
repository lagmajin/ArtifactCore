module;
#include <cstddef>
#include <cstdint>
#include "../Define/DllExportMacro.hpp"

export module Graphics.GPUAppendConsume;

export namespace ArtifactCore {

enum class GPUCounterStatus : std::uint8_t { Valid, Empty, Invalid, Overflow };

struct GPUAtomicCounterPlan {
    GPUCounterStatus status = GPUCounterStatus::Invalid;
    std::size_t initialValue = 0;
    std::size_t capacity = 0;
    std::size_t counterBytes = sizeof(std::uint32_t);
    bool requiresReset = true;
    bool requiresReadback = false;
};

struct GPUAppendConsumePlan {
    GPUCounterStatus status = GPUCounterStatus::Invalid;
    std::size_t elementStrideBytes = 0;
    std::size_t capacity = 0;
    std::size_t dataBytes = 0;
    GPUAtomicCounterPlan counter;
    bool overflowFlagRequired = true;
    bool indirectArgumentsRequired = false;
};

class LIBRARY_DLL_API GPUAppendConsume {
public:
    static GPUAtomicCounterPlan counter(std::size_t capacity,
                                        std::size_t initialValue = 0,
                                        bool requiresReadback = false);
    static GPUAppendConsumePlan append(std::size_t capacity,
                                       std::size_t elementStrideBytes,
                                       bool indirectArguments = false);
    static GPUAppendConsumePlan consume(std::size_t capacity,
                                        std::size_t elementStrideBytes,
                                        bool indirectArguments = false);
};

}
