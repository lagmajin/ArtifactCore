module;
#include <cstdint>
#include <limits>
#include "../../include/Define/DllExportMacro.hpp"

module Graphics.GPUAppendConsume;

namespace ArtifactCore {

GPUAtomicCounterPlan GPUAppendConsume::counter(std::size_t capacity,
                                               std::size_t initialValue,
                                               bool requiresReadback) {
    GPUAtomicCounterPlan plan;
    plan.capacity = capacity;
    plan.initialValue = initialValue;
    plan.requiresReadback = requiresReadback;
    if (capacity == 0) { plan.status = GPUCounterStatus::Empty; return plan; }
    if (initialValue > capacity || capacity > std::numeric_limits<std::uint32_t>::max()) {
        plan.status = GPUCounterStatus::Invalid;
        return plan;
    }
    plan.status = GPUCounterStatus::Valid;
    return plan;
}

GPUAppendConsumePlan GPUAppendConsume::append(std::size_t capacity,
                                              std::size_t stride,
                                              bool indirectArguments) {
    GPUAppendConsumePlan plan;
    plan.capacity = capacity;
    plan.elementStrideBytes = stride;
    plan.indirectArgumentsRequired = indirectArguments;
    plan.counter = counter(capacity);
    if (plan.counter.status != GPUCounterStatus::Valid || stride == 0) {
        plan.status = plan.counter.status == GPUCounterStatus::Valid
            ? GPUCounterStatus::Invalid : plan.counter.status;
        return plan;
    }
    if (capacity > std::numeric_limits<std::size_t>::max() / stride) {
        plan.status = GPUCounterStatus::Overflow;
        return plan;
    }
    plan.dataBytes = capacity * stride;
    plan.status = GPUCounterStatus::Valid;
    return plan;
}

GPUAppendConsumePlan GPUAppendConsume::consume(std::size_t capacity,
                                               std::size_t stride,
                                               bool indirectArguments) {
    auto plan = append(capacity, stride, indirectArguments);
    plan.counter.requiresReset = false;
    return plan;
}

}
