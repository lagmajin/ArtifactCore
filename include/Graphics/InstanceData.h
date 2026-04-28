// InstanceData.h
// Mesh instancing instance data structure
// Compatible with DiligentEngine Structured Buffer

#ifndef ARTIFACT_CORE_GRAPHICS_INSTANCE_DATA_H
#define ARTIFACT_CORE_GRAPHICS_INSTANCE_DATA_H

#include <cstdint>

namespace ArtifactCore {

struct InstanceData {
    float transform[16];   // 4x4 matrix, column-major
    float color[4];        // RGBA
    float weight;          // Effector influence (0.0 - 1.0)
    float timeOffset;      // Time remap offset in seconds
    float padding[2];     // 16-byte alignment (total 64 bytes)
};

} // namespace ArtifactCore

#endif // ARTIFACT_CORE_GRAPHICS_INSTANCE_DATA_H
