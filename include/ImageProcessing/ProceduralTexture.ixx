module;

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <QString>

#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>

#include "../Define/DllExportMacro.hpp"

export module ImageProcessing.ProceduralTexture;

import Image.ImageF32x4_RGBA;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore
{
enum class ProceduralTextureGeneratorKind : std::uint32_t
{
    Perlin = 0,
    Simplex = 1,
    FBM = 2,
    Voronoi = 3,
    White = 4,
    Value = 5,
    Gradient = 6,
};

enum class ProceduralTextureVoronoiMode : std::uint32_t
{
    Distance = 0,
    Cell = 1,
    Edge = 2,
};

enum class ProceduralTextureGradientMode : std::uint32_t
{
    Linear = 0,
    Radial = 1,
};

enum class ProceduralTextureBlendMode : std::uint32_t
{
    Add = 0,
    Multiply = 1,
    Overlay = 2,
};

enum class ProceduralTextureOutputFormat : std::uint32_t
{
    Rgba8 = 1,
    Float32 = 2,
    Both = 3,
};

struct ProceduralTextureGeneratorParams
{
    ProceduralTextureGeneratorKind kind = ProceduralTextureGeneratorKind::Perlin;
    ProceduralTextureVoronoiMode voronoiMode = ProceduralTextureVoronoiMode::Distance;
    ProceduralTextureGradientMode gradientMode = ProceduralTextureGradientMode::Linear;

    std::uint32_t seed = 0;
    std::array<float, 2> scale { 8.0f, 8.0f };
    std::array<float, 2> offset { 0.0f, 0.0f };
    float rotation = 0.0f;
    float amplitude = 1.0f;
    std::uint32_t octaves = 1;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float cellJitter = 0.75f;
};

struct ProceduralTexturePostProcess
{
    bool normalize = false;
    float normalizeMin = 0.0f;
    float normalizeMax = 1.0f;

    bool invert = false;

    bool clampEnabled = true;
    float clampMin = 0.0f;
    float clampMax = 1.0f;

    bool remapEnabled = false;
    float remapInMin = 0.0f;
    float remapInMax = 1.0f;
    float remapOutMin = 0.0f;
    float remapOutMax = 1.0f;

    float gamma = 1.0f;

    ProceduralTextureBlendMode blendMode = ProceduralTextureBlendMode::Add;
    float blendWeight = 0.5f;
    bool useSecondary = false;
    ProceduralTextureGeneratorParams secondary {};

    bool domainWarpEnabled = false;
    ProceduralTextureGeneratorParams warp {};
    float warpAmplitude = 0.25f;
};

struct ProceduralTextureSettings
{
    int width = 1024;
    int height = 1024;
    bool seamless = true;
    bool parallel = false;
    ProceduralTextureOutputFormat outputFormat = ProceduralTextureOutputFormat::Both;
    ProceduralTextureGeneratorParams primary {};
    ProceduralTexturePostProcess post {};
};

enum class ProceduralTexturePreset : std::uint32_t
{
    Marble = 0,
    Clouds = 1,
    Cellular = 2,
    Fabric = 3,
    Terrain = 4,
    Metal = 5,
};

struct ProceduralTextureOutput
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba8;
    std::vector<float> rgba32f;

    bool hasRgba8() const { return !rgba8.empty(); }
    bool hasFloat32() const { return !rgba32f.empty(); }
    void clear();
    std::unique_ptr<ImageF32x4_RGBA> toFloatImage() const;
};

class LIBRARY_DLL_API ProceduralTextureGenerator
{
public:
    static ProceduralTextureOutput generate(const ProceduralTextureSettings& settings);
    static bool generate(const ProceduralTextureSettings& settings, ImageF32x4_RGBA& output);
    static ProceduralTextureSettings makePreset(ProceduralTexturePreset preset, std::uint32_t seed = 0);
};

class LIBRARY_DLL_API ProceduralTextureComputePipeline
{
public:
    explicit ProceduralTextureComputePipeline(GpuContext& context);
    ~ProceduralTextureComputePipeline();

    bool initialize();
    bool generate(IDeviceContext* ctx, ITextureView* outputUAV, const ProceduralTextureSettings& settings);
    bool ready() const;

    static ITexture* createOutputTexture(IRenderDevice* device,
                                         std::uint32_t width,
                                         std::uint32_t height,
                                         TEXTURE_FORMAT format = TEX_FORMAT_RGBA8_UNORM,
                                         const char* name = "ProceduralTexture");

private:
    struct Impl;
    GpuContext& context_;
    Impl* pImpl_ = nullptr;
};
}
