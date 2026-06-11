// RayTrace.hlsl
// Diligent Engine DXR ray tracing shaders

struct RayPayload
{
    float3 color;
    int depth;
};

// Shader Resources
RaytracingAccelerationStructure g_TLAS : register(t0);
RWTexture2D<float4> g_Output : register(u0);

cbuffer CameraCB : register(b0)
{
    float4x4 g_ViewProjInv;
    float4 g_CameraOrigin;
    int g_MaxDepth;
};

[shader("raygeneration")]
void RayGen()
{
    uint2 launchId = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchId) + 0.5f) / float2(launchSize);
    float2 d = uv * 2.0f - 1.0f;
    d.y = -d.y; // Flip Y for typical UV space mapping

    // Transform screen coords to world ray direction
    float4 target = mul(g_ViewProjInv, float4(d, 1.0f, 1.0f));
    target.xyz /= target.w;
    float3 rayDir = normalize(target.xyz - g_CameraOrigin.xyz);

    RayDesc ray;
    ray.Origin = g_CameraOrigin.xyz;
    ray.Direction = rayDir;
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;

    RayPayload payload;
    payload.color = float3(0, 0, 0);
    payload.depth = g_MaxDepth;

    TraceRay(g_TLAS, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

    g_Output[launchId] = float4(payload.color, 1.0f);
}

[shader("miss")]
void PrimaryMiss(inout RayPayload payload)
{
    // Background gradient mapping to match CPU raytracer
    float3 unitDir = WorldRayDirection();
    float t = 0.5f * (unitDir.y + 1.0f);
    payload.color = (1.0f - t) * float3(1.0f, 1.0f, 1.0f) + t * float3(0.5f, 0.7f, 1.0f);
}

// Custom Intersection or Closest Hit for Sphere procedural geometries.
// Since spheres are custom primitives, DXR uses intersection shaders for spheres,
// or we can represent them as mesh bounds. For simplicity, we define a Closest Hit.
[shader("closesthit")]
void SphereClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Simple debug shading for the sphere geometry hit
    payload.color = float3(0.8f, 0.2f, 0.2f);
}
