module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include "../Define/DllExportMacro.hpp"
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include "InstanceData.h"
export module Graphics.MeshRenderer;



import Graphics.GPUcomputeContext;
import Frame.Debug;

export namespace ArtifactCore {

using namespace Diligent;

/**
 * @brief Mesh instancing renderer using DiligentEngine
 * Provides GPU instancing similar to ParticleRenderer but for arbitrary meshes.
 */
class LIBRARY_DLL_API MeshRenderer {
public:
    MeshRenderer(GpuContext& context);
    ~MeshRenderer();

    /**
     * @brief Initialize renderer with maximum instance count and mesh reference
     * @param maxInstances Maximum number of instances to support
     * @param vertexCount Number of vertices in the mesh
     * @param indexCount Number of indices in the mesh (0 for non-indexed)
     */
    void initialize(size_t maxInstances, size_t vertexCount, size_t indexCount);
    
    void setFrameCostStats(ArtifactCore::RenderCostStats* stats);

    /**
     * @brief Upload mesh geometry to GPU buffers
     * @param positions Pointer to float3 positions (x,y,z per vertex)
     * @param normals Pointer to float3 normals (optional, can be nullptr)
     * @param uvs Pointer to float2 uvs (optional, can be nullptr)
     * @param indices Pointer to uint32 indices (optional for non-indexed)
     */
    void updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                            const uint32_t* indices);

    /**
     * @brief Upload instance data to GPU structured buffer
     * @param instances Pointer to instance data array
     * @param count Number of instances to upload
     */
    void updateInstanceData(const InstanceData* instances, size_t count);

    /**
     * @brief Prepare for rendering (set PSO, bind resources)
     */
    void prepare(IDeviceContext* pContext);

    /**
     * @brief Issue draw commands
     * @param pContext Device context
     * @param instanceCount Number of instances to draw
     */
    void draw(IDeviceContext* pContext, size_t instanceCount);

    // Matrix setters
    void setViewMatrix(const float* matrix);   // float[16]
    void setProjectionMatrix(const float* matrix); // float[16]

private:
    GpuContext& context_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxInstances_ = 0;
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;

    struct ShaderConstants {
        float viewMatrix[16];
        float projMatrix[16];
    };
    ShaderConstants constants_;
    ArtifactCore::RenderCostStats* frameCostStats_ = nullptr;

    void createPSO();
    void createBuffers();
};

} // namespace ArtifactCore
