module;
#include <memory>
#include <vector>
#include <iostream>
#include <cstring>

#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Buffer.h>
#include <Texture.h>
#include <PipelineState.h>
#include <RefCntAutoPtr.hpp>
#include <BottomLevelAS.h>
#include <TopLevelAS.h>
#include <ShaderBindingTable.h>
#include <GraphicsTypesX.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>

module Render.GPURayTracer;

import Graphics.GPUcomputeContext;

namespace ArtifactCore::RayTrace
{

class GPURayTracer::Impl
{
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> rtPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderBindingTable> sbt;
    
    Diligent::RefCntAutoPtr<Diligent::IBottomLevelAS> sphereBLAS;
    Diligent::RefCntAutoPtr<Diligent::ITopLevelAS> sceneTLAS;

    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTexture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> outputUAV;
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> cameraConstantBuffer;

    bool initialized = false;
    bool rtSupported = false;

    struct CameraConstants
    {
        Diligent::float4x4 viewProjInv;
        Diligent::float4 cameraOrigin;
        int maxDepth;
        int padding[3];
    };

    Impl()
    {
        ArtifactCore::GpuContext gpuContext;
        gpuContext.Initialize();

        device = gpuContext.RenderDevice();
        context = gpuContext.DeviceContext();

        if (device)
        {
            const auto& props = device->GetDeviceInfo();
            rtSupported = (props.Features.RayTracing != Diligent::DEVICE_FEATURE_STATE_DISABLED);

            if (rtSupported)
            {
                initializeDXR();
            }
        }
    }

    void initializeDXR()
    {
        if (!device) return;

        // 1. Create BLAS (Bottom Level Acceleration Structure) for Sphere
        Diligent::BLASTriangleDesc triangleDesc;
        triangleDesc.GeometryName = "SphereMeshProxy";
        triangleDesc.VertexValueType = Diligent::VT_FLOAT32;
        triangleDesc.VertexComponentCount = 3;
        triangleDesc.MaxVertexCount = 4;
        triangleDesc.IndexType = Diligent::VT_UINT32;
        triangleDesc.MaxPrimitiveCount = 2; 

        Diligent::BottomLevelASDesc blasDesc;
        blasDesc.Name = "Sphere Proxy BLAS";
        blasDesc.TriangleCount = 1;
        blasDesc.pTriangles = &triangleDesc;
        
        device->CreateBLAS(blasDesc, &sphereBLAS);

        // 2. Create TLAS (Top Level Acceleration Structure)
        Diligent::TopLevelASDesc tlasDesc;
        tlasDesc.Name = "Main Scene TLAS";
        tlasDesc.MaxInstanceCount = 1024;
        device->CreateTLAS(tlasDesc, &sceneTLAS);

        // 3. Create Camera Constant Buffer
        Diligent::BufferDesc cbDesc;
        cbDesc.Name = "RayTracingCameraCB";
        cbDesc.Size = sizeof(CameraConstants);
        cbDesc.Usage = Diligent::USAGE_DYNAMIC;
        cbDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        cbDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        device->CreateBuffer(cbDesc, nullptr, &cameraConstantBuffer);

        // 4. Compile Shaders & Setup Pipeline
        initializePipeline();
    }

    void initializePipeline()
    {
        // Setup simple embedded shader loader or compile DXR Shaders.
        // For simplicity and matching codebase style, we compile the HLSL shader code.
        const char* shaderSource = R"(
            RaytracingAccelerationStructure g_TLAS : register(t0);
            RWTexture2D<float4> g_Output : register(u0);

            cbuffer CameraCB : register(b0)
            {
                float4x4 g_ViewProjInv;
                float4 g_CameraOrigin;
                int g_MaxDepth;
            };

            struct RayPayload { float3 color; };

            [shader("raygeneration")]
            void RayGen()
            {
                uint2 launchId = DispatchRaysIndex().xy;
                uint2 launchSize = DispatchRaysDimensions().xy;
                float2 uv = (float2(launchId) + 0.5f) / float2(launchSize);
                g_Output[launchId] = float4(uv, 0.7f, 1.0f);
            }

            [shader("miss")]
            void PrimaryMiss() {}
        )";

        Diligent::ShaderCreateInfo shaderCI;
        shaderCI.Source = shaderSource;
        shaderCI.SourceLength = std::strlen(shaderSource);
        shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        shaderCI.ShaderCompiler = Diligent::SHADER_COMPILER_DXC;
        shaderCI.HLSLVersion = {6, 3};

        Diligent::RefCntAutoPtr<Diligent::IShader> rayGenShader;
        shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_RAY_GEN;
        shaderCI.Desc.Name = "RayTrace RayGen";
        shaderCI.EntryPoint = "RayGen";
        device->CreateShader(shaderCI, &rayGenShader);

        Diligent::RefCntAutoPtr<Diligent::IShader> missShader;
        shaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_RAY_MISS;
        shaderCI.Desc.Name = "RayTrace Miss";
        shaderCI.EntryPoint = "PrimaryMiss";
        device->CreateShader(shaderCI, &missShader);

        if (!rayGenShader || !missShader) return;

        Diligent::RayTracingPipelineStateCreateInfoX psoCI("GPURayTracer PSO");
        psoCI.AddGeneralShader("RayGen", rayGenShader);
        psoCI.AddGeneralShader("PrimaryMiss", missShader);
        psoCI.RayTracingPipeline.MaxRecursionDepth = 1;
        psoCI.RayTracingPipeline.ShaderRecordSize = 0;
        psoCI.MaxAttributeSize = 8;
        psoCI.MaxPayloadSize = 16;
        psoCI.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        Diligent::ShaderResourceVariableDesc vars[] = 
        {
            {Diligent::SHADER_TYPE_RAY_GEN, "g_Output", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 1;

        device->CreateRayTracingPipelineState(psoCI, &rtPSO);
        if (!rtPSO) return;

        // Create SBT
        Diligent::ShaderBindingTableDesc sbtDesc;
        sbtDesc.Name = "GPURayTracer SBT";
        sbtDesc.pPSO = rtPSO;
        device->CreateSBT(sbtDesc, &sbt);
        if (sbt)
        {
            sbt->BindRayGenShader("RayGen");
            sbt->BindMissShader("PrimaryMiss", 0);
            context->UpdateSBT(sbt);
        }

        initialized = true;
    }

    void prepareOutputTexture(int width, int height)
    {
        if (outputTexture && outputTexture->GetDesc().Width == static_cast<Diligent::Uint32>(width) && 
            outputTexture->GetDesc().Height == static_cast<Diligent::Uint32>(height))
        {
            return;
        }

        outputUAV.Release();
        outputTexture.Release();

        Diligent::TextureDesc texDesc;
        texDesc.Name = "GPURayTracer Output";
        texDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        texDesc.Usage = Diligent::USAGE_DEFAULT;
        texDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS;
        texDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;

        device->CreateTexture(texDesc, nullptr, &outputTexture);
        if (outputTexture)
        {
            outputUAV = outputTexture->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
            if (auto* var = rtPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_RAY_GEN, "g_Output"))
            {
                var->Set(outputUAV);
            }
        }
    }

    void renderGPU(int width, int height, ImageBuffer& buffer)
    {
        if (!initialized || !device || !context || !sbt)
        {
            // Fail safe fallback render (CPU side)
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    buffer.setPixel(x, y, Color(0.1f, 0.2f, 0.4f), 1);
                }
            }
            return;
        }

        prepareOutputTexture(width, height);

        // Run DXR Dispatch
        Diligent::TraceRaysAttribs traceAttribs{sbt, static_cast<Diligent::Uint32>(width), static_cast<Diligent::Uint32>(height), 1};
        context->TraceRays(traceAttribs);

        // Map Output Back (Headless Readback Staging copy)
        Diligent::TextureDesc stagingDesc;
        stagingDesc.Name = "Readback Staging";
        stagingDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        stagingDesc.Width = width;
        stagingDesc.Height = height;
        stagingDesc.MipLevels = 1;
        stagingDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        stagingDesc.Usage = Diligent::USAGE_STAGING;
        stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
        
        Diligent::RefCntAutoPtr<Diligent::ITexture> stagingTexture;
        device->CreateTexture(stagingDesc, nullptr, &stagingTexture);

        if (stagingTexture)
        {
            Diligent::CopyTextureAttribs copyAttribs(outputTexture, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                                     stagingTexture, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            context->CopyTexture(copyAttribs);
            context->Flush();

            Diligent::MappedTextureSubresource mappedData;
            context->MapTextureSubresource(stagingTexture, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_NONE, nullptr, mappedData);

            const uint8_t* srcPixels = reinterpret_cast<const uint8_t*>(mappedData.pData);
            if (srcPixels)
            {
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        int srcIdx = y * mappedData.Stride + x * 4;
                        float r = srcPixels[srcIdx + 0] / 255.0f;
                        float g = srcPixels[srcIdx + 1] / 255.0f;
                        float b = srcPixels[srcIdx + 2] / 255.0f;
                        buffer.setPixel(x, y, Color(r, g, b), 1);
                    }
                }
            }
            context->UnmapTextureSubresource(stagingTexture, 0, 0);
        }
    }
};

GPURayTracer::GPURayTracer()
    : impl_(std::make_unique<Impl>())
{
    camera = Camera(16.0f / 9.0f, 2.0f, 1.0f, 0.0f,
        Point3(0, 0, 0), Point3(0, 0, -1), Vec3(0, 1, 0));
}

GPURayTracer::~GPURayTracer() = default;

void GPURayTracer::setImageSize(int width, int height)
{
    imageWidth = width;
    imageHeight = height;
}

void GPURayTracer::setSamplesPerPixel(int samples)
{
    samplesPerPixel = samples;
}

void GPURayTracer::setMaxDepth(int depth)
{
    maxDepth = depth;
}

void GPURayTracer::setupCornellBox()
{
    world.clear();
}

void GPURayTracer::setupRandomSpheres(int count)
{
    world.clear();
}

ImageBuffer GPURayTracer::render()
{
    ImageBuffer img(imageWidth, imageHeight);
    impl_->renderGPU(imageWidth, imageHeight, img);
    return img;
}

} // namespace ArtifactCore::RayTrace
