module;
#include <algorithm>
#include <limits>
#include "../../include/Define/DllExportMacro.hpp"

module Graphics.GPUAlgorithms;

namespace ArtifactCore {
namespace {
GPUDispatchPlan makePlan(std::size_t count, std::size_t stride, GPUAlgorithmLimits limits,
                         bool indirect, std::size_t temporary) {
    GPUDispatchPlan plan;
    plan.elementCount = count;
    plan.elementStrideBytes = stride;
    plan.workgroupSize = std::min<std::size_t>(256, limits.maxWorkgroupSize);
    plan.requiresIndirectDispatch = indirect;
    plan.temporaryElementCount = temporary;
    if (count == 0) { plan.status = GPUAlgorithmStatus::EmptyInput; return plan; }
    if (stride == 0 || plan.workgroupSize == 0) { plan.status = GPUAlgorithmStatus::InvalidInput; return plan; }
    if (count > limits.maxElements || count > std::numeric_limits<std::size_t>::max() / stride) {
        plan.status = count > limits.maxElements ? GPUAlgorithmStatus::InvalidInput : GPUAlgorithmStatus::Overflow;
        return plan;
    }
    plan.workgroupCount = (count + plan.workgroupSize - 1) / plan.workgroupSize;
    plan.status = GPUAlgorithmStatus::Valid;
    return plan;
}
}

GPUDispatchPlan GPUAlgorithms::prefixSum(std::size_t count, std::size_t stride, GPUAlgorithmLimits limits) {
    return makePlan(count, stride, limits, false, count);
}

GPUDispatchPlan GPUAlgorithms::compaction(std::size_t count, std::size_t stride, GPUAlgorithmLimits limits) {
    return makePlan(count, stride, limits, true, count);
}

GPUDispatchPlan GPUAlgorithms::radixSort(std::size_t count, std::size_t stride, GPUAlgorithmLimits limits) {
    if (limits.radixBits == 0 || limits.radixBits > 8) {
        GPUDispatchPlan plan; plan.elementCount = count; plan.elementStrideBytes = stride; return plan;
    }
    if (count > std::numeric_limits<std::size_t>::max() / 2) {
        GPUDispatchPlan plan; plan.status = GPUAlgorithmStatus::Overflow; plan.elementCount = count; plan.elementStrideBytes = stride; return plan;
    }
    return makePlan(count, stride, limits, false, count * 2);
}
}
