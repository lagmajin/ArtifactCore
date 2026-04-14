module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Shader.h>
#include <PipelineState.h>
#include <Buffer.h>
#include <Texture.h>
#include <ShaderResourceBinding.h>
#include <RefCntAutoPtr.hpp>
#include <vector>
#include <string>
#include <cmath>

export module Graphics.Raymarching:Engine;

import Graphics.Raymarching;
import Graphics.GPUcomputeContext;
import Color.Float;

namespace ArtifactCore {

using namespace Diligent;

const char* RaymarchingCS = R"(
struct SDFObject {
    int type;
    int op;
    float3 pos;
    float3 rot;
    float3 scale;
    float4 color;
    float smoothness;
    float3 extra;
};

struct SceneData {
    float4x4 ViewInv;
    float4x4 ProjInv;
    float4 Params; // x=maxDist, y=minDist, z=maxSteps, w=objCount
    float4 Lighting; // xyz=lightDir, w=unused
};

StructuredBuffer<SDFObject> Objects : register(t0);
ConstantBuffer<SceneData> Scene : register(b0);
RWTexture2D<float4> Output : register(u0);

// --- SDF Primitives ---
float sdSphere(float3 p, float s) { return length(p) - s; }
float sdBox(float3 p, float3 b) {
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdTorus(float3 p, float2 t) {
    float2 q = float2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

// --- Operations ---
float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

// --- Scene Setup ---
float GetDist(float3 p, out float4 outColor) {
    float d = 1e10;
    outColor = float4(1,1,1,1);
    int count = (int)Scene.Params.w;
    
    for(int i=0; i<count; ++i) {
        SDFObject obj = Objects[i];
        float3 localP = p - obj.pos;
        // Simplified: rotation not handled for brevity in this initial drop
        
        float d_obj = 1e10;
        if(obj.type == 0) d_obj = sdSphere(localP, obj.scale.x);
        else if(obj.type == 1) d_obj = sdBox(localP, obj.scale);
        else if(obj.type == 2) d_obj = sdTorus(localP, obj.scale.xy);
        
        if(obj.op == 0) { // Union
            if(d_obj < d) { d = d_obj; outColor = obj.color; }
        } else if(obj.op == 3) { // Smooth Union
            float oldD = d;
            d = opSmoothUnion(d, d_obj, obj.smoothness);
            float h = clamp(0.5 + 0.5 * (d_obj - oldD) / obj.smoothness, 0.0, 1.0);
            outColor = lerp(obj.color, outColor, h);
        }
        // ... Intersect/Subtract ...
    }
    return d;
}

float3 GetNormal(float3 p) {
    float4 dummy;
    float d = GetDist(p, dummy);
    float2 e = float2(0.01, 0);
    float3 n = d - float3(
        GetDist(p - e.xyy, dummy),
        GetDist(p - e.yxy, dummy),
        GetDist(p - e.yyx, dummy));
    return normalize(n);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint2 size;
    Output.GetDimensions(size.x, size.y);
    if(DTid.x >= size.x || DTid.y >= size.y) return;

    float2 uv = (float2(DTid.xy) + 0.5) / float2(size);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y *= -1.0;

    // Ray Gen
    float4 near = mul(Scene.ProjInv, float4(ndc, 0.0, 1.0));
    near /= near.w;
    float4 far = mul(Scene.ProjInv, float4(ndc, 1.0, 1.0));
    far /= far.w;
    
    float3 rd = normalize(mul((float3x3)Scene.ViewInv, far.xyz - near.xyz));
    float3 ro = mul(Scene.ViewInv, float4(0,0,0,1)).xyz;

    // Raymarch
    float t = 0;
    float4 color = float4(0,0,0,0);
    bool hit = false;
    for(int i=0; i<(int)Scene.Params.z; ++i) {
        float3 p = ro + rd * t;
        float4 objColor;
        float d = GetDist(p, objColor);
        if(d < Scene.Params.y) {
            hit = true;
            color = objColor;
            break;
        }
        t += d;
        if(t > Scene.Params.x) break;
    }

    if(hit) {
        float3 p = ro + rd * t;
        float3 n = GetNormal(p);
        float diff = max(0.1, dot(n, normalize(Scene.Lighting.xyz)));
        Output[DTid.xy] = float4(color.rgb * diff, 1.0);
    } else {
        Output[DTid.xy] = float4(0, 0, 0.1, 0.0); // Background
    }
}
)";

class RaymarchingEngine : public IRaymarchingEngine {
public:
    RaymarchingEngine() = default;
    virtual ~RaymarchingEngine() { destroy(); }

    bool initialize(GpuContext* context) override {
        if (!context) return false;
        device_ = context->D3D12RenderDevice();
        
        // Create Constant Buffer
        BufferDesc cbDesc;
        cbDesc.Name = "Raymarching Scene CB";
        cbDesc.Size = 4096; // Overkill for SceneData but safe
        cbDesc.Usage = USAGE_DYNAMIC;
        cbDesc.BindFlags = BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device_->CreateBuffer(cbDesc, nullptr, &sceneCB_);

        // Create PSO
        ComputePipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "Raymarching PSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
        
        RefCntAutoPtr<IShader> pCS;
        ShaderCreateInfo shaderCI;
        shaderCI.Source = RaymarchingCS;
        shaderCI.EntryPoint = "main";
        shaderCI.Desc.Name = "Raymarching Compute Shader";
        shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        device_->CreateShader(shaderCI, &pCS);
        
        psoCI.pCS = pCS;
        device_->CreateComputePipelineState(psoCI, &pso_);
        pso_->CreateShaderResourceBinding(&srb_, true);

        return true;
    }

    void destroy() override {
        srb_.Release();
        pso_.Release();
        sceneCB_.Release();
        objectBuffer_.Release();
        device_.Release();
    }

    void render(IDeviceContext* pContext,
                const std::vector<SDFObject>& scene,
                ITextureView* pOutputUAV,
                const float* viewMatrix,
                const float* projMatrix,
                const RaymarchingParams& params) override {
        
        if (!pso_ || !pContext || scene.empty()) return;

        // 1. Update Objects Structured Buffer
        if (!objectBuffer_ || currentObjectCapacity_ < scene.size()) {
            objectBuffer_.Release();
            currentObjectCapacity_ = (static_cast<Uint32>(scene.size()) + 63) & ~63;
            BufferDesc sbDesc;
            sbDesc.Name = "SDF Object SB";
            sbDesc.Size = sizeof(SDFObject) * currentObjectCapacity_;
            sbDesc.Usage = USAGE_DYNAMIC;
            sbDesc.BindFlags = BIND_SHADER_RESOURCE;
            sbDesc.Mode = BUFFER_MODE_STRUCTURED;
            sbDesc.ElementByteStride = sizeof(SDFObject);
            sbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            device_->CreateBuffer(sbDesc, nullptr, &objectBuffer_);
            srb_->GetVariableByName(SHADER_TYPE_COMPUTE, "Objects")->Set(objectBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        }

        {
            void* pData = nullptr;
            pContext->MapBuffer(objectBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
            if (pData) {
                std::memcpy(pData, scene.data(), sizeof(SDFObject) * scene.size());
                pContext->UnmapBuffer(objectBuffer_, MAP_WRITE);
            }
        }

        // 2. Update Scene Constants
        {
            struct SceneData {
                float viewInv[16];
                float projInv[16];
                float params[4];
                float lighting[4];
            } data;
            
            std::memcpy(data.viewInv, viewMatrix, 64);
            std::memcpy(data.projInv, projMatrix, 64);
            data.params[0] = params.maxDistance;
            data.params[1] = params.minDistance;
            data.params[2] = (float)params.maxSteps;
            data.params[3] = (float)scene.size();
            
            data.lighting[0] = 0.5f; data.lighting[1] = 1.0f; data.lighting[2] = 0.5f; data.lighting[3] = 1.0f;

            void* pData = nullptr;
            pContext->MapBuffer(sceneCB_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
            if (pData) {
                std::memcpy(pData, &data, sizeof(data));
                pContext->UnmapBuffer(sceneCB_, MAP_WRITE);
            }
        }

        // 3. Dispatch
        srb_->GetVariableByName(SHADER_TYPE_COMPUTE, "Output")->Set(pOutputUAV);
        srb_->GetVariableByName(SHADER_TYPE_COMPUTE, "Scene")->Set(sceneCB_);
        
        pContext->SetPipelineState(pso_);
        pContext->CommitShaderResources(srb_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        
        const auto& outDesc = pOutputUAV->GetTexture()->GetDesc();
        pContext->DispatchCompute(DispatchComputeAttribs((outDesc.Width + 7) / 8, (outDesc.Height + 7) / 8, 1));
    }

private:
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IPipelineState> pso_;
    RefCntAutoPtr<IShaderResourceBinding> srb_;
    RefCntAutoPtr<IBuffer> sceneCB_;
    RefCntAutoPtr<IBuffer> objectBuffer_;
    Uint32 currentObjectCapacity_ = 0;
};

std::unique_ptr<IRaymarchingEngine> createRaymarchingEngine() {
    return std::make_unique<RaymarchingEngine>();
}

} // namespace ArtifactCore
