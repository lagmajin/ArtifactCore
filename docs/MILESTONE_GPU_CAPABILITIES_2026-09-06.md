**最終更新:** 2026-09-06

# GPU Capability Foundation

## Scope

GPU algorithms and the future GPU thread pool need a backend-neutral contract
for synchronization, timeline progress, memory pressure, and indirect count.
This milestone adds only the Core-side contract. It does not enable Vulkan
extensions directly, change Diligent device creation, or alter an existing
render path.

## Implemented

- `GPUCapabilitySnapshot` for synchronization2, timeline semaphore, memory
  budget, indirect count, subgroup operations, and descriptor indexing.
- `GPUMemoryBudget` normalization and safe usability check.
- Monotonic `GPUTimelineToken` creation with overflow guard.
- `GPUIndirectCountPlan` validation.
- C++20 module and CMake manifest registration.
- `GpuContext::capabilities()` now exposes the initial Diligent-backed adapter
  mapping for indirect draw capability, bindless resources, and an approximate
  local+unified memory capacity.

## Deferred

- Querying the active Diligent/Vulkan device.
- Vulkan extension enablement and D3D12 mapping.
- Timeline semaphore/fence submission.
- Runtime indirect draw/dispatch integration.
- Hardware validation and performance measurement.

The initial mapping intentionally leaves synchronization2 and timeline
semaphore false until the active Diligent backend exposes verified capability
and submission contracts. Adapter memory is a capacity estimate, not current
VRAM usage.
