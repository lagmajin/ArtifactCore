module;
#include <utility>
#include <DiligentCore/Common/interface/BasicMath.hpp>
export module VertexBuffer;

export namespace ArtifactCore
{
 using namespace Diligent;

    struct RectVertex
    {
        float2 position;
        float4 color;
    };

    struct SpriteVertex
    {
        float2 position;
        float2 uv;
        float4 color;
    };
 
    struct DotLineVertex
    {
        float2 position;
        float4 color;
        float  dist;
    };

    struct GizmoVertex3D
    {
        float3 position;
        float4 color;
    };
}
