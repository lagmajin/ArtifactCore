module;
#include <vector>
#include <memory>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>

export module Graphics.Raymarching;

import Color.Float;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

    using namespace Diligent;

    enum class SDFType : int {
        Sphere = 0,
        Box = 1,
        Torus = 2,
        Cylinder = 3,
        Capsule = 4
    };

    enum class SDFOp : int {
        Union = 0,
        Subtract = 1,
        Intersect = 2,
        SmoothUnion = 3
    };

    export struct SDFObject {
        SDFType type = SDFType::Sphere;
        SDFOp op = SDFOp::Union;
        float position[3] = {0, 0, 0};
        float rotation[3] = {0, 0, 0};
        float scale[3] = {1, 1, 1};
        FloatColor color = {1, 1, 1, 1};
        float smoothness = 0.5f;
        float extra[3] = {0, 0, 0}; // Type-specific params
    };

    export struct RaymarchingParams {
        float maxDistance = 100.0f;
        float minDistance = 0.001f;
        int maxSteps = 128;
        bool enableShadows = true;
        bool enableAO = true;
        float ambientOcclusionIntensity = 0.5f;
    };

    export class IRaymarchingEngine {
    public:
        virtual ~IRaymarchingEngine() = default;

        virtual bool initialize(GpuContext* context) = 0;
        virtual void destroy() = 0;

        /**
         * @brief Renders the SDF scene into the output texture.
         * @param scene Array of objects.
         * @param output Destination UAV (must have BIND_UNORDERED_ACCESS).
         * @param viewMatrix Camera's view matrix.
         * @param projMatrix Camera's projection matrix (or just FOV if preferred).
         * @param params Raymarching settings.
         */
        virtual void render(IDeviceContext* pContext,
                           const std::vector<SDFObject>& scene,
                           ITextureView* pOutputUAV,
                           const float* viewMatrix,
                           const float* projMatrix,
                           const RaymarchingParams& params) = 0;
    };

    export std::unique_ptr<IRaymarchingEngine> createRaymarchingEngine();

} // namespace ArtifactCore
