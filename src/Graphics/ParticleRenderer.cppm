module;
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineStateCache.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QDebug>

module Graphics.ParticleRenderer;

import Frame.Debug;
import Graphics.Compute;

namespace ArtifactCore {

// One compiled graphics pipeline per render-options variant. Entries bind
// the constants buffer generation they were built against; buffer recreation
// retires old entries without an explicit purge pass.
struct ParticleGraphicsPsoCacheEntry {
    ParticleRenderOptions options;
    uint64_t bufferGeneration = 0;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
};

struct ParticleRenderer::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pSRB_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pParticleBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pConstantBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pIndirectArgsBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pCompactedParticleBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pCullConstantsBuffer_;
    std::unique_ptr<ComputeExecutor>                           pCullExecutor_;
    bool indirectDrawSupported_ = false;
    bool gpuCullReady_ = false;
    bool gpuCullActive_ = false;
    // Latched cull-build failure: without this a persistently failing
    // compile would be retried (a full dxc invocation) on every eligible
    // frame. Reset together with gpuCullReady_ when buffers are recreated.
    bool gpuCullBuildFailed_ = false;
    // Bumped on every createBuffers(); compiled pipelines bind one specific
    // constants buffer, so results from older generations are discarded.
    uint64_t bufferGeneration_ = 0;
    // Installed graphics pipelines by options (GUI thread only).
    std::vector<ParticleGraphicsPsoCacheEntry> graphicsPsoCache_;
    // Async worker state. The worker only touches job/result slots under
    // asyncMutex_; installation into live members happens in pumpAsyncResults
    // on the GUI thread.
    std::thread asyncThread_;
    std::mutex asyncMutex_;
    std::condition_variable asyncCv_;
    bool asyncStop_ = false;
    bool asyncDisabled_ = false;
    bool graphicsJobPending_ = false;
    ParticleRenderOptions graphicsJobOptions_;
    uint64_t graphicsJobBufferGen_ = 0;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> graphicsJobConstants_;
    bool graphicsResultReady_ = false;
    bool graphicsResultOk_ = false;
    ParticleRenderOptions graphicsResultOptions_;
    uint64_t graphicsResultBufferGen_ = 0;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> graphicsResultPso_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> graphicsResultSrb_;
    bool cullJobPending_ = false;
    uint64_t cullJobBufferGen_ = 0;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> cullJobConstants_;
    bool cullResultReady_ = false;
    bool cullResultOk_ = false;
    uint64_t cullResultBufferGen_ = 0;
    std::unique_ptr<ComputeExecutor> cullResultExecutor_;
    // Graphics failure latch (GUI thread only): stops re-requesting a build
    // that already failed for these exact options. Expires automatically
    // when options or the buffer generation change.
    bool hasGraphicsFailure_ = false;
    ParticleRenderOptions graphicsFailedOptions_;
    uint64_t graphicsFailedBufferGen_ = 0;

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock(asyncMutex_);
            asyncStop_ = true;
        }
        asyncCv_.notify_all();
        if (asyncThread_.joinable()) {
            asyncThread_.join();
        }
    }
};

struct ParticleCullConstants {
    float modelMatrix[16] = {};
    float viewMatrix[16] = {};
    float projMatrix[16] = {};
    Uint32 inputCount = 0;
    Uint32 outputCapacity = 0;
    Uint32 padding[2] = {};
};

namespace {

// Bump when the embedded particle HLSL changes: backends key stored blobs
// by bytecode too, but this retires poisoned files unconditionally.
constexpr int kParticlePsoCacheFileVersion = 1;

struct DevicePsoCacheEntry {
    Diligent::RefCntAutoPtr<Diligent::IPipelineStateCache> cache;
    QString filePath;
    bool dirty = false;
    std::chrono::steady_clock::time_point lastSave{};
};

std::mutex& DevicePsoCacheMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::map<const void*, DevicePsoCacheEntry>& DevicePsoCacheMap()
{
    static std::map<const void*, DevicePsoCacheEntry> map;
    return map;
}

QString ParticlePsoCacheFilePath(IRenderDevice* device)
{
    QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    int backend = static_cast<int>(RENDER_DEVICE_TYPE_UNDEFINED);
    // FNV-1a over the adapter description (bounded, no Qt hash dependency).
    unsigned int adapterHash = 2166136261u;
    if (device) {
        backend = static_cast<int>(device->GetDeviceInfo().Type);
        const auto& adapterInfo = device->GetAdapterInfo();
        for (int i = 0; i < 128 && adapterInfo.Description[i] != '\0'; ++i) {
            adapterHash ^= static_cast<unsigned int>(adapterInfo.Description[i]);
            adapterHash *= 16777619u;
        }
        adapterHash ^= static_cast<unsigned int>(adapterInfo.VendorId) * 2654435761u;
    }
    return QStringLiteral("%1/pso_cache/particle_%2_%3_v%4.bin")
        .arg(base).arg(backend)
        .arg(adapterHash, 8, 16, QLatin1Char('0'))
        .arg(kParticlePsoCacheFileVersion);
}

// Returns the process-wide pipeline-state cache for a device (possibly
// null on backends without support, or when the disk blob is unusable).
// The registry owns the object; callers use it transiently during PSO
// creation only.
Diligent::RefCntAutoPtr<Diligent::IPipelineStateCache> FindDevicePsoCache(
    IRenderDevice* device)
{
    if (!device) {
        return {};
    }
    std::lock_guard<std::mutex> lock(DevicePsoCacheMutex());
    auto& map = DevicePsoCacheMap();
    const void* key = static_cast<const void*>(device);
    auto it = map.find(key);
    if (it != map.end()) {
        return it->second.cache;
    }
    DevicePsoCacheEntry entry;
    entry.filePath = ParticlePsoCacheFilePath(device);
    QByteArray diskBlob;
    QFile file(entry.filePath);
    if (file.open(QIODevice::ReadOnly)) {
        diskBlob = file.readAll();
        file.close();
    }
    PipelineStateCacheCreateInfo cacheInfo;
    cacheInfo.Desc.Mode = PSO_CACHE_MODE_LOAD_STORE;
    if (!diskBlob.isEmpty()) {
        cacheInfo.pCacheData = diskBlob.constData();
        // IDataBlob size is size_t; Diligent takes Uint32 (4GB+ caches
        // cannot exist here — a handful of particle PSOs).
        cacheInfo.CacheDataSize =
            static_cast<Uint32>(std::min<size_t>(
                static_cast<size_t>(diskBlob.size()),
                static_cast<size_t>(std::numeric_limits<Uint32>::max())));
    }
    RefCntAutoPtr<IPipelineStateCache> cache;
    device->CreatePipelineStateCache(cacheInfo, &cache);
    // Unsupported backends (GL/WebGPU stubs) return null: builds proceed
    // without a cache exactly as before.
    entry.cache = cache;
    map.emplace(key, std::move(entry));
    return cache;
}

void MarkDevicePsoCacheDirty(IRenderDevice* device)
{
    if (!device) {
        return;
    }
    std::lock_guard<std::mutex> lock(DevicePsoCacheMutex());
    auto& map = DevicePsoCacheMap();
    const auto it = map.find(static_cast<const void*>(device));
    if (it != map.end() && it->second.cache) {
        it->second.dirty = true;
    }
}

// Serializes newly stored pipelines, throttled: GetData + file write runs
// on the calling (GUI) thread, so this must stay infrequent.
void MaybeSaveDevicePsoCache(IRenderDevice* device)
{
    if (!device) {
        return;
    }
    RefCntAutoPtr<IPipelineStateCache> cache;
    QString filePath;
    {
        std::lock_guard<std::mutex> lock(DevicePsoCacheMutex());
        auto& map = DevicePsoCacheMap();
        const auto it = map.find(static_cast<const void*>(device));
        if (it == map.end() || !it->second.cache || !it->second.dirty) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (it->second.lastSave != std::chrono::steady_clock::time_point{} &&
            now - it->second.lastSave < std::chrono::seconds(120)) {
            return;
        }
        it->second.lastSave = now;
        it->second.dirty = false;
        cache = it->second.cache;
        filePath = it->second.filePath;
    }
    RefCntAutoPtr<IDataBlob> blob;
    cache->GetData(&blob);
    if (!blob || blob->GetSize() == 0) {
        return;
    }
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[ParticleRenderer] PSO cache save failed:" << filePath;
        return;
    }
    const auto* bytes = static_cast<const char*>(blob->GetConstDataPtr());
    qint64 remaining = static_cast<qint64>(blob->GetSize());
    while (remaining > 0) {
        const qint64 written = file.write(bytes, remaining);
        if (written <= 0) {
            qWarning() << "[ParticleRenderer] PSO cache save failed:" << filePath;
            return;
        }
        bytes += written;
        remaining -= written;
    }
}

} // namespace

const char* ParticleCullCSSource = R"(
struct ParticleData {
    // float4 keeps the stride backend-independent: float3+float3 packs
    // tightly under DXIL but pads to 16-byte alignment under Vulkan std430,
    // which shifted color/size reads (orange rendered as cyan, flickering
    // quads). The C++ ParticleVertex carries explicit padding to the same
    // 96 bytes.
    float4 position; // xyz + padding
    float4 velocity; // xyz + padding
    float4 color;
    float size;
    float stretch;
    float rotation;
    float age;
    float lifetime;
    int spriteFrame;
    int spriteRows;
    int spriteCols;
    // Trail head (previous position). Read only on the CPU line path;
    // present here so the stride matches the C++ ParticleVertex (96 bytes)
    // on every backend.
    float4 prevPosition;
};
StructuredBuffer<ParticleData> g_Input : register(t0);
RWStructuredBuffer<ParticleData> g_Output : register(u0);
RWStructuredBuffer<uint> g_Args : register(u1);
cbuffer CullConstants : register(b0) {
    float4 ModelRow0; float4 ModelRow1; float4 ModelRow2; float4 ModelRow3;
    float4 ViewRow0; float4 ViewRow1; float4 ViewRow2; float4 ViewRow3;
    float4 ProjRow0; float4 ProjRow1; float4 ProjRow2; float4 ProjRow3;
    uint InputCount;
    uint OutputCapacity;
    uint2 Padding;
};
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= InputCount) return;
    ParticleData p = g_Input[id.x];
    if (p.age >= p.lifetime || p.color.a <= 0.0 || p.size <= 0.0) return;
    float4 localPos = float4(p.position.xyz, 1.0);
    float4 worldPos = float4(dot(localPos, ModelRow0), dot(localPos, ModelRow1),
                             dot(localPos, ModelRow2), dot(localPos, ModelRow3));
    float4 viewPos = float4(dot(worldPos, ViewRow0), dot(worldPos, ViewRow1),
                           dot(worldPos, ViewRow2), dot(worldPos, ViewRow3));
    float4 clip = float4(dot(viewPos, ProjRow0), dot(viewPos, ProjRow1),
                         dot(viewPos, ProjRow2), dot(viewPos, ProjRow3));
    // Cull margin must cover the drawn quad half-extent (VS halfWidth is
    // size*10, halfHeight scales by stretch) or edge particles pop in/out.
    float margin = max(2.0, p.size * max(1.0, p.stretch) * 10.0);
    bool visible = clip.w > 0.00001 &&
        clip.x >= -clip.w - margin && clip.x <= clip.w + margin &&
        clip.y >= -clip.w - margin && clip.y <= clip.w + margin &&
        clip.z >= -margin && clip.z <= clip.w + margin;
    if (!visible) return;
    uint dst;
    InterlockedAdd(g_Args[1], 1, dst);
    if (dst < OutputCapacity) g_Output[dst] = p;
}
)";

const char* ParticleVSSource = R"(
struct ParticleData {
    // xyz + padding (see cull CS comment): 96-byte stride on every backend.
    float4 position;
    float4 velocity;
    float4 color;
    float  size;
    float  stretch;
    float  rotation;
    float  age;
    float  lifetime;
    // Sprite fields are unused by the vertex stage, but the StructuredBuffer
    // stride must match the C++ ParticleVertex (96 bytes) and the cull CS
    // layout, otherwise every particle after the first reads shifted data.
    int    spriteFrame;
    int    spriteRows;
    int    spriteCols;
    // Trail head (previous position). Unused by the shaders; stride only.
    float4 prevPosition;
};

StructuredBuffer<ParticleData> g_Particles : register(t0);

cbuffer Constants : register(b0) {
    float4 ModelRow0;
    float4 ModelRow1;
    float4 ModelRow2;
    float4 ModelRow3;
    float4 ViewRow0;
    float4 ViewRow1;
    float4 ViewRow2;
    float4 ViewRow3;
    float4 ProjRow0;
    float4 ProjRow1;
    float4 ProjRow2;
    float4 ProjRow3;
    float DeltaTime;
    int BillboardMode;
    float2 Padding;
};

struct VS_Input {
    uint VertexID   : SV_VertexID;
    uint InstanceID : SV_InstanceID;
};

struct PS_Input {
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
    // x = quad half width, y = quad half height, z = stretch factor.
    float3 Shape : TEXCOORD1;
};

static const float2 c_Offsets[4] = {
    float2(-0.5, -0.5), float2(-0.5, 0.5),
    float2(0.5, -0.5), float2(0.5, 0.5)
};

PS_Input VSMain(VS_Input In) {
    PS_Input Out;
    ParticleData p = g_Particles[In.InstanceID];
    
    float4 localPos = float4(p.position.xyz, 1.0);
    float4 worldPos = float4(
        dot(localPos, ModelRow0),
        dot(localPos, ModelRow1),
        dot(localPos, ModelRow2),
        dot(localPos, ModelRow3));
    float4 viewPos = float4(
        dot(worldPos, ViewRow0),
        dot(worldPos, ViewRow1),
        dot(worldPos, ViewRow2),
        dot(worldPos, ViewRow3));
    
    // Rotation (degrees to radians)
    // VelocityAligned (BillboardMode 3) must use view-space velocity so the
    // sprite tilts along the on-screen motion direction. World-space
    // atan2(velocity.y, velocity.x) is only correct while the camera looks
    // straight down -Z; after an orbit it points elsewhere. The direction
    // rows match the position transform convention above (w = 0).
    float3 viewVelocity = float3(
        dot(p.velocity.xyz, ViewRow0.xyz),
        dot(p.velocity.xyz, ViewRow1.xyz),
        dot(p.velocity.xyz, ViewRow2.xyz));
    float rotationDegrees = p.rotation;
    if (BillboardMode == 3 && dot(viewVelocity.xy, viewVelocity.xy) > 0.000001) {
        rotationDegrees += atan2(viewVelocity.y, viewVelocity.x) * 180.0 / 3.14159265;
    }
    float rad = rotationDegrees * 3.14159265 / 180.0;
    float cosR = cos(rad);
    float sinR = sin(rad);
    
    // c_Offsets is +-0.5 and localOffset is scaled by halfWidth*2, so the quad
    // spans +-halfWidth: the full width is 2*halfWidth. The software path draws
    // drawEllipse with radius scale*10 (diameter 20*scale), so halfWidth must be
    // size*10 for the GPU quad width to match that diameter.
    float halfWidth = max(0.375, p.size * 10.0);
    float halfHeight = halfWidth * max(1.0, p.stretch);
    float2 localOffset = c_Offsets[In.VertexID] * float2(halfWidth * 2.0, halfHeight * 2.0);
    float2 rotatedOffset;
    rotatedOffset.x = localOffset.x * cosR - localOffset.y * sinR;
    rotatedOffset.y = localOffset.x * sinR + localOffset.y * cosR;
    
    if (BillboardMode == 0) {
        localPos.xy += rotatedOffset;
        worldPos = float4(
            dot(localPos, ModelRow0),
            dot(localPos, ModelRow1),
            dot(localPos, ModelRow2),
            dot(localPos, ModelRow3));
        viewPos = float4(
            dot(worldPos, ViewRow0),
            dot(worldPos, ViewRow1),
            dot(worldPos, ViewRow2),
            dot(worldPos, ViewRow3));
    } else {
        viewPos.xy += rotatedOffset;
    }
    
    Out.Pos = float4(
        dot(viewPos, ProjRow0),
        dot(viewPos, ProjRow1),
        dot(viewPos, ProjRow2),
        dot(viewPos, ProjRow3));
    Out.UV = c_Offsets[In.VertexID] + 0.5;
    Out.Color = p.color;
    Out.Shape = float3(halfWidth, halfHeight, max(1.0, p.stretch));
    
    return Out;
}
)";

const char* ParticlePSSource = R"(
struct PS_Input {
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
    // x = quad half width, y = quad half height, z = stretch factor.
    float3 Shape : TEXCOORD1;
};

float4 PSMain(PS_Input In) : SV_Target {
    if (In.Shape.z > 1.05) {
        // Stretched capsule matching the software path: rounded rect with a
        // vertical linear gradient (transparent -> solid -> transparent).
        // UV.y = 0/1 are the transparent ends, like the CPU gradient stops.
        float2 halfExtent = max(In.Shape.xy, float2(0.0001, 0.0001));
        float radius = halfExtent.x;
        float2 local = (In.UV - 0.5) * (halfExtent * 2.0);
        float2 q = abs(local) - (halfExtent - radius);
        float dist = length(max(q, 0.0)) - radius;
        if (dist > 0.0) discard;
        float t = saturate(In.UV.y);
        float4 transparent = float4(In.Color.rgb, 0.0);
        float4 grad = t < 0.15 ? lerp(transparent, In.Color, t / 0.15)
                    : (t < 0.85 ? In.Color
                                : lerp(In.Color, transparent,
                                       (t - 0.85) / 0.15));
        return grad;
    }
    float dist = length(In.UV - 0.5);
    if (dist > 0.5) discard;
    
    // Soft circle (edge0 < edge1 is required: reversed smoothstep args are
    // undefined behavior in HLSL and flicker/disappear on some drivers)
    float alpha = 1.0 - smoothstep(0.4, 0.5, dist);
    return float4(In.Color.rgb, In.Color.a * alpha);
}
)";

ParticleRenderer::ParticleRenderer(GpuContext& context)
    : context_(context), pImpl_(new Impl())
{
    pImpl_->pCullExecutor_ = std::make_unique<ComputeExecutor>(context_);
    try {
        pImpl_->asyncThread_ = std::thread([this] { asyncWorkerMain(); });
    } catch (const std::system_error&) {
        // No background compilation: prepare() falls back to synchronous
        // builds instead of stalling layer addition with nothing to wait on.
        pImpl_->asyncDisabled_ = true;
    }
    debugState_ = DebugState::Constructed;
}
ParticleRenderer::~ParticleRenderer()
{
    delete pImpl_;
}

void ParticleRenderer::initialize(size_t maxParticles) {
    maxParticles_ = maxParticles;
    constants_.billboardMode = static_cast<int>(renderOptions_.billboard);
    debugState_ = DebugState::Initialized;
    debugMax_ = static_cast<qulonglong>(maxParticles_);
    createBuffers();
    // No synchronous PSO compilation here: the first prepare() requests an
    // async build and the CPU fallback covers the interim frames.
}

void ParticleRenderer::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    frameCostStats_ = stats;
}

void ParticleRenderer::createBuffers() {
    auto pDevice = context_.RenderDevice();
    pImpl_->gpuCullReady_ = false;
    pImpl_->gpuCullActive_ = false;
    pImpl_->gpuCullBuildFailed_ = false;
    ++pImpl_->bufferGeneration_;
    pImpl_->graphicsPsoCache_.clear();
    {
        // Drop queued-but-unstarted builds: their results would be stale
        // (tagged with the previous buffer generation) by design, so don't
        // burn a dxc invocation on them. An in-flight build finishes
        // harmlessly and is discarded by generation on publish.
        std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
        pImpl_->graphicsJobPending_ = false;
        pImpl_->graphicsJobConstants_.Release();
        pImpl_->cullJobPending_ = false;
        pImpl_->cullJobConstants_.Release();
    }
    if (!pDevice || maxParticles_ == 0) {
        debugState_ = DebugState::BuffersSkipped;
        debugMax_ = static_cast<qulonglong>(maxParticles_);
        debugFlagA_ = pDevice != nullptr;
        qWarning() << "[ParticleRenderer] createBuffers() skipped"
                   << "device=" << (pDevice != nullptr)
                   << "maxParticles=" << maxParticles_;
        return;
    }

    // 1. Particle Structured Buffer
    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle Structured Buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.Size              = sizeof(ParticleVertex) * maxParticles_;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(ParticleVertex);
    pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pParticleBuffer_);
    BuffDesc.Name = "Particle Compacted Structured Buffer";
    BuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    pDevice->CreateBuffer(
        BuffDesc, nullptr, &pImpl_->pCompactedParticleBuffer_);

    // 2. Constant Buffer
    BuffDesc.Name              = "Particle Constants CB";
    BuffDesc.Usage             = USAGE_DYNAMIC;
    BuffDesc.Size              = sizeof(ShaderConstants);
    BuffDesc.BindFlags         = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
    BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
    BuffDesc.ElementByteStride = 0;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pConstantBuffer_);
    pImpl_->indirectDrawSupported_ =
        (pDevice->GetAdapterInfo().DrawCommand.CapFlags &
         DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT) != 0;
    if (pImpl_->indirectDrawSupported_) {
        BufferDesc indirectDesc;
        indirectDesc.Name = "Particle Indirect Draw Args";
        indirectDesc.Usage = USAGE_DEFAULT;
        indirectDesc.Size = sizeof(Uint32) * 4;
        indirectDesc.BindFlags =
            BIND_INDIRECT_DRAW_ARGS | BIND_UNORDERED_ACCESS;
        indirectDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        indirectDesc.Mode = BUFFER_MODE_STRUCTURED;
        indirectDesc.ElementByteStride = sizeof(Uint32);
        pDevice->CreateBuffer(
            indirectDesc, nullptr, &pImpl_->pIndirectArgsBuffer_);
        pImpl_->indirectDrawSupported_ =
            pImpl_->pIndirectArgsBuffer_ != nullptr;
    }
    BufferDesc cullConstantsDesc;
    cullConstantsDesc.Name = "Particle Cull Constants";
    cullConstantsDesc.Usage = USAGE_DYNAMIC;
    cullConstantsDesc.Size = sizeof(ParticleCullConstants);
    cullConstantsDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cullConstantsDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(
        cullConstantsDesc, nullptr, &pImpl_->pCullConstantsBuffer_);
    debugState_ = DebugState::BuffersReady;
    debugMax_ = static_cast<qulonglong>(maxParticles_);
    debugFlagA_ = pImpl_->pParticleBuffer_ != nullptr;
    debugFlagB_ = pImpl_->pConstantBuffer_ != nullptr;
}

namespace {

// Shared dxc compile + graphics PSO creation, callable from any thread:
// touches only the device and the out-params, never live renderer members.
bool BuildParticleGraphicsPipeline(
    GpuContext& context, const ParticleRenderOptions& options,
    IBuffer* constantsBuffer,
    RefCntAutoPtr<IPipelineState>& outPso,
    RefCntAutoPtr<IShaderResourceBinding>& outSrb)
{
    outPso.Release();
    outSrb.Release();
    auto pDevice = context.RenderDevice();
    if (!pDevice || !constantsBuffer) {
        qWarning() << "[ParticleRenderer] graphics build skipped"
                   << "device=" << (pDevice != nullptr)
                   << "constantBuffer=" << (constantsBuffer != nullptr);
        return false;
    }
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Particle Rendering PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Use Triangle Strip for 4 vertices
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = DefaultParticleRTVFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = options.depthTest;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = options.depthWrite;

    // Alpha blending (Additive by default for many particle effects, or Normal)
    auto& RT0 = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = true;
    RT0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend = BLEND_FACTOR_ONE;
    RT0.BlendOp = BLEND_OPERATION_ADD;
    switch (options.blend) {
    case ParticleBlendPolicy::Subtractive:
        RT0.BlendOp = BLEND_OPERATION_REV_SUBTRACT;
        break;
    case ParticleBlendPolicy::Alpha:
        RT0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        break;
    case ParticleBlendPolicy::Screen:
        RT0.SrcBlend = BLEND_FACTOR_ONE;
        RT0.DestBlend = BLEND_FACTOR_INV_SRC_COLOR;
        break;
    case ParticleBlendPolicy::Multiply:
        RT0.SrcBlend = BLEND_FACTOR_DEST_COLOR;
        RT0.DestBlend = BLEND_FACTOR_ZERO;
        break;
    case ParticleBlendPolicy::Additive:
    default:
        break;
    }
    RT0.SrcBlendAlpha  = BLEND_FACTOR_ONE;
    RT0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOpAlpha   = BLEND_OPERATION_ADD;
    RT0.RenderTargetWriteMask = COLOR_MASK_ALL;

    // Compile Shaders (output-param style per new GPUComputeContext API)
    RefCntAutoPtr<IShader> vs, ps;
    context.CompileShader(ParticleVSSource, SHADER_TYPE_VERTEX, "VSMain", &vs);
    context.CompileShader(ParticlePSSource, SHADER_TYPE_PIXEL,  "PSMain", &ps);
    if (!vs || !ps) {
        qWarning("[ParticleRenderer] particle shader compilation FAILED");
        return false;
    }

    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;

    // Process-wide Diligent pipeline-state cache (disk-backed): hits skip
    // the driver-side PSO compile on repeat runs. Null on unsupported
    // backends, which simply keeps the previous behavior. The registry owns
    // the object for the process lifetime, so the raw pointer stays valid.
    auto devicePsoCache = FindDevicePsoCache(pDevice);
    PSOCreateInfo.pPSOCache = devicePsoCache.RawPtr();

    // Layout
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    static std::array<ShaderResourceVariableDesc, 1> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();

    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &outPso);

    if (!outPso) {
        qWarning("[ParticleRenderer] PSO creation FAILED — "
                 "check shader compilation and RTV format");
        return false;
    }

    // Bind Constants cbuffer (static variable — bound once at PSO level)
    auto* pConstVar = outPso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (!pConstVar) {
        qWarning("[ParticleRenderer] 'Constants' cbuffer not found in PSO "
                 "— static variable name mismatch");
        outPso.Release();
        return false;
    }
    pConstVar->Set(constantsBuffer);
    outPso->CreateShaderResourceBinding(&outSrb, true);
    if (!outSrb) {
        qWarning("[ParticleRenderer] PSO shader-resource binding FAILED");
        outPso.Release();
        return false;
    }
    MarkDevicePsoCacheDirty(pDevice);
    return true;
}

// Shared cull compute pipeline creation. The executor is fully worker-local
// until published: safe to run on the async thread.
bool BuildParticleCullPipeline(
    GpuContext& context, ComputeExecutor& executor, IBuffer* constantsBuffer)
{
    if (!constantsBuffer) {
        return false;
    }
    static std::array<ShaderResourceVariableDesc, 3> CullVars = {{
        {SHADER_TYPE_COMPUTE, "g_Input", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Output", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_Args", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    ComputePipelineDesc cullDesc;
    cullDesc.name = "Particle GPU Visibility Cull";
    cullDesc.shaderSource = ParticleCullCSSource;
    cullDesc.entryPoint = "CSMain";
    cullDesc.variables = CullVars.data();
    cullDesc.variableCount = static_cast<Uint32>(CullVars.size());
    cullDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    auto pDevice = context.RenderDevice();
    auto devicePsoCache = FindDevicePsoCache(pDevice);
    cullDesc.psoCache = devicePsoCache.RawPtr();
    const bool built = executor.build(cullDesc) &&
        executor.setBuffer("CullConstants", constantsBuffer) &&
        executor.createShaderResourceBinding(true);
    if (built) {
        MarkDevicePsoCacheDirty(pDevice);
    }
    return built;
}

template <typename RendererImpl>
void InsertGraphicsCache(
    RendererImpl* impl, const ParticleRenderOptions& options,
    uint64_t bufferGeneration,
    RefCntAutoPtr<IPipelineState>& pso,
    RefCntAutoPtr<IShaderResourceBinding>& srb)
{
    for (auto& entry : impl->graphicsPsoCache_) {
        if (entry.options == options &&
            entry.bufferGeneration == bufferGeneration) {
            entry.pso = pso;
            entry.srb = srb;
            return;
        }
    }
    if (impl->graphicsPsoCache_.size() >= 8) {
        // Retire stale generations first; entries are tiny apart from the
        // device-owned pipeline objects they reference.
        const uint64_t liveGeneration = impl->bufferGeneration_;
        impl->graphicsPsoCache_.erase(
            std::remove_if(impl->graphicsPsoCache_.begin(),
                           impl->graphicsPsoCache_.end(),
                           [liveGeneration](const ParticleGraphicsPsoCacheEntry& entry) {
                               return entry.bufferGeneration != liveGeneration;
                           }),
            impl->graphicsPsoCache_.end());
    }
    if (impl->graphicsPsoCache_.size() >= 8) {
        impl->graphicsPsoCache_.clear();
    }
    ParticleGraphicsPsoCacheEntry entry;
    entry.options = options;
    entry.bufferGeneration = bufferGeneration;
    entry.pso = pso;
    entry.srb = srb;
    impl->graphicsPsoCache_.push_back(std::move(entry));
}

} // namespace

void ParticleRenderer::markPsoReady()
{
    debugState_ = DebugState::PsoReady;
    debugMax_ = static_cast<qulonglong>(maxParticles_);
    debugFlagA_ = pImpl_->pPSO_ != nullptr;
    debugFlagB_ = pImpl_->pSRB_ != nullptr;
    debugFlagC_ = renderOptions_.depthWrite;
    debugCount_ = pImpl_->gpuCullReady_ ? 1 : 0;
    debugA_ = static_cast<qulonglong>(renderOptions_.blend);
    debugB_ = renderOptions_.depthTest ? 1 : 0;
}

bool ParticleRenderer::ensureCullPipeline()
{
    if (pImpl_->gpuCullReady_) {
        return true;
    }
    if (pImpl_->gpuCullBuildFailed_) {
        return false;
    }
    if (!pImpl_->pCullExecutor_ || !pImpl_->pCullConstantsBuffer_) {
        return false;
    }
    pImpl_->gpuCullReady_ = BuildParticleCullPipeline(
        context_, *pImpl_->pCullExecutor_,
        pImpl_->pCullConstantsBuffer_.RawPtr());
    if (!pImpl_->gpuCullReady_) {
        pImpl_->gpuCullBuildFailed_ = true;
        qWarning() << "[ParticleRenderer] GPU cull pipeline build failed"
                   << "— continuing with direct/indirect draws";
    } else {
        MaybeSaveDevicePsoCache(context_.RenderDevice());
    }
    return pImpl_->gpuCullReady_;
}

void ParticleRenderer::buildGraphicsSync()
{
    RefCntAutoPtr<IPipelineState> pso;
    RefCntAutoPtr<IShaderResourceBinding> srb;
    if (!BuildParticleGraphicsPipeline(context_, renderOptions_,
                                        pImpl_->pConstantBuffer_.RawPtr(),
                                        pso, srb)) {
        pImpl_->hasGraphicsFailure_ = true;
        pImpl_->graphicsFailedOptions_ = renderOptions_;
        pImpl_->graphicsFailedBufferGen_ = pImpl_->bufferGeneration_;
        debugState_ = DebugState::PsoFailed;
        debugMax_ = static_cast<qulonglong>(maxParticles_);
        return;
    }
    InsertGraphicsCache(pImpl_, renderOptions_, pImpl_->bufferGeneration_,
                        pso, srb);
    pImpl_->pPSO_ = pso;
    pImpl_->pSRB_ = srb;
    pImpl_->hasGraphicsFailure_ = false;
    markPsoReady();
    qDebug() << "[ParticleRenderer] PSO created successfully";
    MaybeSaveDevicePsoCache(context_.RenderDevice());
}

bool ParticleRenderer::useCachedGraphicsPso()
{
    for (auto& entry : pImpl_->graphicsPsoCache_) {
        if (entry.options == renderOptions_ &&
            entry.bufferGeneration == pImpl_->bufferGeneration_ &&
            entry.pso && entry.srb) {
            pImpl_->pPSO_ = entry.pso;
            pImpl_->pSRB_ = entry.srb;
            return true;
        }
    }
    if (pImpl_->asyncDisabled_) {
        buildGraphicsSync();
        for (auto& entry : pImpl_->graphicsPsoCache_) {
            if (entry.options == renderOptions_ &&
                entry.bufferGeneration == pImpl_->bufferGeneration_ &&
                entry.pso && entry.srb) {
                pImpl_->pPSO_ = entry.pso;
                pImpl_->pSRB_ = entry.srb;
                return true;
            }
        }
        return false;
    }
    requestAsyncGraphics();
    return false;
}

bool ParticleRenderer::ensureGraphicsPipeline(
    const ParticleRenderOptions& options)
{
    setRenderOptions(options);
    for (auto& entry : pImpl_->graphicsPsoCache_) {
        if (entry.options == renderOptions_ &&
            entry.bufferGeneration == pImpl_->bufferGeneration_ &&
            entry.pso && entry.srb) {
            pImpl_->pPSO_ = entry.pso;
            pImpl_->pSRB_ = entry.srb;
            return true;
        }
    }
    pumpAsyncResults();
    for (auto& entry : pImpl_->graphicsPsoCache_) {
        if (entry.options == renderOptions_ &&
            entry.bufferGeneration == pImpl_->bufferGeneration_ &&
            entry.pso && entry.srb) {
            pImpl_->pPSO_ = entry.pso;
            pImpl_->pSRB_ = entry.srb;
            return true;
        }
    }
    if (pImpl_->hasGraphicsFailure_ &&
        pImpl_->graphicsFailedOptions_ == renderOptions_ &&
        pImpl_->graphicsFailedBufferGen_ == pImpl_->bufferGeneration_) {
        return false;
    }

    // Export is a cold, serial frame path. Build the one missing PSO here so
    // ParticleLayer can choose its existing CPU fallback before queueing GPU
    // work. Interactive rendering keeps the asynchronous path unchanged.
    buildGraphicsSync();
    for (auto& entry : pImpl_->graphicsPsoCache_) {
        if (entry.options == renderOptions_ &&
            entry.bufferGeneration == pImpl_->bufferGeneration_ &&
            entry.pso && entry.srb) {
            pImpl_->pPSO_ = entry.pso;
            pImpl_->pSRB_ = entry.srb;
            return true;
        }
    }
    return false;
}

void ParticleRenderer::requestAsyncGraphics()
{
    requestAsyncBuild(renderOptions_);
}

void ParticleRenderer::requestAsyncBuild(const ParticleRenderOptions& options)
{
    auto pDevice = context_.RenderDevice();
    if (!pDevice || maxParticles_ == 0 || !pImpl_->pConstantBuffer_) {
        return;
    }
    if (pImpl_->hasGraphicsFailure_ &&
        pImpl_->graphicsFailedOptions_ == options &&
        pImpl_->graphicsFailedBufferGen_ == pImpl_->bufferGeneration_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
        if (pImpl_->graphicsJobPending_ &&
            pImpl_->graphicsJobOptions_ == options &&
            pImpl_->graphicsJobBufferGen_ == pImpl_->bufferGeneration_) {
            return;
        }
        pImpl_->graphicsJobOptions_ = options;
        pImpl_->graphicsJobBufferGen_ = pImpl_->bufferGeneration_;
        pImpl_->graphicsJobConstants_ = pImpl_->pConstantBuffer_;
        pImpl_->graphicsJobPending_ = true;
    }
    pImpl_->asyncCv_.notify_one();
}

void ParticleRenderer::prewarmCommonPipelines()
{
    if (pImpl_->asyncDisabled_ || maxParticles_ == 0 ||
        !context_.RenderDevice() || !pImpl_->pConstantBuffer_) {
        return;
    }
    // The default additive pipeline covers fire/explosion/spark/rain-type
    // presets; Alpha covers normal-blend layers and form particles. Cached
    // or failed builds are skipped, so repeated calls are cheap.
    ParticleRenderOptions additive;
    ParticleRenderOptions alpha = additive;
    alpha.blend = ParticleBlendPolicy::Alpha;
    const ParticleRenderOptions wanted[2] = {additive, alpha};
    for (const auto& options : wanted) {
        bool cached = false;
        for (const auto& entry : pImpl_->graphicsPsoCache_) {
            if (entry.options == options &&
                entry.bufferGeneration == pImpl_->bufferGeneration_ &&
                entry.pso && entry.srb) {
                cached = true;
                break;
            }
        }
        if (!cached) {
            requestAsyncBuild(options);
        }
    }
    if (!pImpl_->gpuCullReady_ && !pImpl_->gpuCullBuildFailed_) {
        requestAsyncCull();
    }
}

void ParticleRenderer::requestAsyncCull()
{
    if (!pImpl_->pCullExecutor_ || !pImpl_->pCullConstantsBuffer_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
        if (pImpl_->cullJobPending_ &&
            pImpl_->cullJobBufferGen_ == pImpl_->bufferGeneration_) {
            return;
        }
        pImpl_->cullJobBufferGen_ = pImpl_->bufferGeneration_;
        pImpl_->cullJobConstants_ = pImpl_->pCullConstantsBuffer_;
        pImpl_->cullJobPending_ = true;
    }
    pImpl_->asyncCv_.notify_one();
}

void ParticleRenderer::pumpAsyncResults()
{
    bool hasGraphics = false;
    ParticleRenderOptions graphicsOptions;
    uint64_t graphicsBufferGen = 0;
    bool graphicsOk = false;
    RefCntAutoPtr<IPipelineState> graphicsPso;
    RefCntAutoPtr<IShaderResourceBinding> graphicsSrb;
    bool hasCull = false;
    bool cullOk = false;
    uint64_t cullBufferGen = 0;
    std::unique_ptr<ComputeExecutor> cullExecutor;
    {
        std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
        if (pImpl_->graphicsResultReady_) {
            graphicsOptions = pImpl_->graphicsResultOptions_;
            graphicsBufferGen = pImpl_->graphicsResultBufferGen_;
            graphicsOk = pImpl_->graphicsResultOk_;
            graphicsPso = std::move(pImpl_->graphicsResultPso_);
            graphicsSrb = std::move(pImpl_->graphicsResultSrb_);
            pImpl_->graphicsResultReady_ = false;
            hasGraphics = true;
        }
        if (pImpl_->cullResultReady_) {
            cullOk = pImpl_->cullResultOk_;
            cullBufferGen = pImpl_->cullResultBufferGen_;
            cullExecutor = std::move(pImpl_->cullResultExecutor_);
            pImpl_->cullResultReady_ = false;
            hasCull = true;
        }
    }
    if (hasGraphics) {
        if (graphicsOk && graphicsPso && graphicsSrb &&
            graphicsBufferGen == pImpl_->bufferGeneration_) {
            InsertGraphicsCache(pImpl_, graphicsOptions, graphicsBufferGen,
                                graphicsPso, graphicsSrb);
            pImpl_->hasGraphicsFailure_ = false;
            if (graphicsOptions == renderOptions_) {
                pImpl_->pPSO_ = graphicsPso;
                pImpl_->pSRB_ = graphicsSrb;
                markPsoReady();
                qDebug() << "[ParticleRenderer] PSO created successfully";
            }
        } else if (!graphicsOk &&
                   graphicsBufferGen == pImpl_->bufferGeneration_) {
            pImpl_->hasGraphicsFailure_ = true;
            pImpl_->graphicsFailedOptions_ = graphicsOptions;
            pImpl_->graphicsFailedBufferGen_ = graphicsBufferGen;
            qWarning() << "[ParticleRenderer] async PSO build failed"
                       << "— staying on the CPU path for these options";
        }
        // Stale generations are dropped silently; prepare() re-requests
        // while they are still needed.
    }
    if (hasCull) {
        if (cullOk && cullExecutor &&
            cullBufferGen == pImpl_->bufferGeneration_) {
            pImpl_->pCullExecutor_.swap(cullExecutor);
            pImpl_->gpuCullReady_ = true;
        } else if (!cullOk && cullBufferGen == pImpl_->bufferGeneration_) {
            pImpl_->gpuCullBuildFailed_ = true;
            qWarning() << "[ParticleRenderer] async GPU cull build failed"
                       << "— continuing with direct/indirect draws";
        }
    }
    if ((hasGraphics && graphicsOk) || (hasCull && cullOk)) {
        // Newly stored driver pipelines are persisted for the next process
        // start (throttled inside).
        MaybeSaveDevicePsoCache(context_.RenderDevice());
    }
}

void ParticleRenderer::asyncWorkerMain()
{
    for (;;) {
        ParticleRenderOptions graphicsOptions;
        uint64_t graphicsBufferGen = 0;
        RefCntAutoPtr<IBuffer> graphicsConstants;
        bool doGraphics = false;
        uint64_t cullBufferGen = 0;
        RefCntAutoPtr<IBuffer> cullConstants;
        bool doCull = false;
        {
            std::unique_lock<std::mutex> lock(pImpl_->asyncMutex_);
            pImpl_->asyncCv_.wait(lock, [this] {
                return pImpl_->asyncStop_ || pImpl_->graphicsJobPending_ ||
                    pImpl_->cullJobPending_;
            });
            if (pImpl_->asyncStop_) {
                return;
            }
            if (pImpl_->graphicsJobPending_) {
                graphicsOptions = pImpl_->graphicsJobOptions_;
                graphicsBufferGen = pImpl_->graphicsJobBufferGen_;
                graphicsConstants = pImpl_->graphicsJobConstants_;
                pImpl_->graphicsJobConstants_.Release();
                pImpl_->graphicsJobPending_ = false;
                doGraphics = true;
            }
            if (pImpl_->cullJobPending_) {
                cullBufferGen = pImpl_->cullJobBufferGen_;
                cullConstants = pImpl_->cullJobConstants_;
                pImpl_->cullJobConstants_.Release();
                pImpl_->cullJobPending_ = false;
                doCull = true;
            }
        }
        if (doGraphics && graphicsConstants) {
            RefCntAutoPtr<IPipelineState> pso;
            RefCntAutoPtr<IShaderResourceBinding> srb;
            const bool ok = BuildParticleGraphicsPipeline(
                context_, graphicsOptions, graphicsConstants.RawPtr(),
                pso, srb);
            {
                std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
                pImpl_->graphicsResultOptions_ = graphicsOptions;
                pImpl_->graphicsResultBufferGen_ = graphicsBufferGen;
                pImpl_->graphicsResultOk_ = ok;
                pImpl_->graphicsResultPso_ = std::move(pso);
                pImpl_->graphicsResultSrb_ = std::move(srb);
                pImpl_->graphicsResultReady_ = true;
            }
        }
        if (doCull && cullConstants) {
            auto executor = std::make_unique<ComputeExecutor>(context_);
            const bool ok = BuildParticleCullPipeline(
                context_, *executor, cullConstants.RawPtr());
            {
                std::lock_guard<std::mutex> lock(pImpl_->asyncMutex_);
                pImpl_->cullResultOk_ = ok;
                pImpl_->cullResultBufferGen_ = cullBufferGen;
                pImpl_->cullResultExecutor_ = std::move(executor);
                pImpl_->cullResultReady_ = true;
            }
        }
    }
}

void ParticleRenderer::updateBuffer(const ParticleRenderData& data) {
    setRenderOptions(data.options);
    debugCount_ = static_cast<qulonglong>(data.particles.size());
    debugUploaded_ = 0;
    debugMax_ = static_cast<qulonglong>(maxParticles_);
    if (data.particles.empty()) {
        lastUploadedParticleCount_ = 0;
        debugState_ = DebugState::UpdateEmpty;
        return;
    }
    auto pContext = context_.DeviceContext();
    
    size_t count = std::min(data.particles.size(), maxParticles_);
    lastUploadedParticleCount_ = count;
    debugUploaded_ = static_cast<qulonglong>(count);
    if (!pContext || !pImpl_->pParticleBuffer_ || count == 0) {
        lastUploadedParticleCount_ = 0;
        debugState_ = DebugState::UpdateSkipped;
        debugFlagA_ = pContext != nullptr;
        debugFlagB_ = pImpl_->pParticleBuffer_ != nullptr;
        qWarning() << "[ParticleRenderer] updateBuffer skipped"
                   << "ctx=" << (pContext != nullptr)
                   << "particleBuffer=" << (pImpl_->pParticleBuffer_ != nullptr)
                   << "count=" << data.particles.size()
                   << "uploaded=" << count
                   << "max=" << maxParticles_;
        return;
    }
    pContext->UpdateBuffer(pImpl_->pParticleBuffer_, 0, sizeof(ParticleVertex) * count, 
                          data.particles.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    debugState_ = DebugState::BufferUpdated;
    if (frameCostStats_) {
        ++frameCostStats_->bufferUpdates;
    }
}

void ParticleRenderer::setRenderOptions(const ParticleRenderOptions& options)
{
    if (renderOptions_ == options) {
        return;
    }
    renderOptions_ = options;
    constants_.billboardMode = static_cast<int>(renderOptions_.billboard);
    // No synchronous rebuild: prepare() serves the matching cached pipeline
    // or requests an async build, so option flips never stall the GUI thread.
}

size_t ParticleRenderer::lastUploadedParticleCount() const
{
    return lastUploadedParticleCount_;
}

void ParticleRenderer::prepare(IDeviceContext* pContext) {
    prepared_ = false;
    pumpAsyncResults();
    if (!pContext || !pImpl_->pConstantBuffer_) {
        debugState_ = DebugState::PrepareSkippedContext;
        debugFlagA_ = pContext != nullptr;
        debugFlagB_ = pImpl_->pConstantBuffer_ != nullptr;
        debugFlagC_ = false;
        return;
    }
    if (!useCachedGraphicsPso()) {
        // Async pipeline build in flight (or failed and latched): the layer
        // CPU fallback covers these frames. No per-frame warning — waiting
        // for a background compile is a normal transient, not an error.
        debugState_ = DebugState::PrepareWaitingPipeline;
        debugFlagA_ = pImpl_->asyncDisabled_;
        debugFlagB_ = false;
        debugFlagC_ = false;
        return;
    }

    // Update Constants
    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) {
        // Do not commit resources or reuse the previous frame's cull state
        // when the current frame's constants could not be uploaded.
        pImpl_->gpuCullActive_ = false;
        debugState_ = DebugState::PrepareSkippedConstantMap;
        qWarning() << "[ParticleRenderer] prepare() skipped: constant buffer map failed";
        return;
    }
    memcpy(pData, &constants_, sizeof(ShaderConstants));
    pContext->UnmapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE);
    if (frameCostStats_) {
        ++frameCostStats_->bufferUpdates;
    }

    const bool cullRequested = pImpl_->indirectDrawSupported_ &&
        pImpl_->pCompactedParticleBuffer_ && pImpl_->pIndirectArgsBuffer_ &&
        lastUploadedParticleCount_ >= 64 &&
        renderOptions_.blend == ParticleBlendPolicy::Additive;
    // The cull compute pipeline compiles on first actual use. The async
    // worker handles it when available; otherwise build it inline (legacy
    // synchronous path, only when the worker thread could not start).
    if (cullRequested && !pImpl_->gpuCullReady_ && !pImpl_->gpuCullBuildFailed_) {
        if (pImpl_->asyncDisabled_) {
            ensureCullPipeline();
        } else {
            requestAsyncCull();
        }
    }
    pImpl_->gpuCullActive_ = pImpl_->gpuCullReady_ && cullRequested;
    if (pImpl_->gpuCullActive_) {
        const Uint32 args[4] = {4u, 0u, 0u, 0u};
        pContext->UpdateBuffer(
            pImpl_->pIndirectArgsBuffer_, 0, sizeof(args), args,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ParticleCullConstants cullConstants;
        std::memcpy(cullConstants.modelMatrix, constants_.modelMatrix,
                    sizeof(cullConstants.modelMatrix));
        std::memcpy(cullConstants.viewMatrix, constants_.viewMatrix,
                    sizeof(cullConstants.viewMatrix));
        std::memcpy(cullConstants.projMatrix, constants_.projMatrix,
                    sizeof(cullConstants.projMatrix));
        cullConstants.inputCount =
            static_cast<Uint32>(lastUploadedParticleCount_);
        cullConstants.outputCapacity = static_cast<Uint32>(maxParticles_);
        void* cullData = nullptr;
        pContext->MapBuffer(
            pImpl_->pCullConstantsBuffer_, MAP_WRITE, MAP_FLAG_DISCARD,
            cullData);
        if (cullData) {
            std::memcpy(cullData, &cullConstants, sizeof(cullConstants));
            pContext->UnmapBuffer(
                pImpl_->pCullConstantsBuffer_, MAP_WRITE);
        } else {
            pImpl_->gpuCullActive_ = false;
        }
        if (pImpl_->gpuCullActive_) {
            const bool inputBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Input", pImpl_->pParticleBuffer_->GetDefaultView(
                               BUFFER_VIEW_SHADER_RESOURCE));
            const bool outputBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Output", pImpl_->pCompactedParticleBuffer_->GetDefaultView(
                                BUFFER_VIEW_UNORDERED_ACCESS));
            const bool argsBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Args", pImpl_->pIndirectArgsBuffer_->GetDefaultView(
                              BUFFER_VIEW_UNORDERED_ACCESS));
            pImpl_->gpuCullActive_ =
                inputBound && outputBound && argsBound;
            if (pImpl_->gpuCullActive_) {
                DispatchComputeAttribs dispatch;
                dispatch.ThreadGroupCountX =
                    (static_cast<Uint32>(lastUploadedParticleCount_) + 63u) /
                    64u;
                pImpl_->pCullExecutor_->dispatch(
                    pContext, dispatch,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }
    }

    // Set SRV
    auto* pParticleVar = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles");
    auto* drawParticleBuffer = pImpl_->gpuCullActive_
        ? pImpl_->pCompactedParticleBuffer_.RawPtr()
        : pImpl_->pParticleBuffer_.RawPtr();
    auto* pParticleSRV = drawParticleBuffer
        ? drawParticleBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE)
        : nullptr;
    if (!pParticleVar || !pParticleSRV) {
        debugState_ = DebugState::PrepareSkippedBinding;
        debugFlagA_ = pParticleVar != nullptr;
        debugFlagB_ = pParticleSRV != nullptr;
        qWarning() << "[ParticleRenderer] prepare() skipped: particle SRV binding unavailable"
                   << "particleVar=" << (pParticleVar != nullptr)
                   << "particleSRV=" << (pParticleSRV != nullptr);
        return;
    }
    pParticleVar->Set(pParticleSRV);

    pContext->SetPipelineState(pImpl_->pPSO_);
    if (frameCostStats_) {
        ++frameCostStats_->psoSwitches;
        ++frameCostStats_->srbCommits;
    }
    pContext->CommitShaderResources(pImpl_->pSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    prepared_ = true;
    debugState_ = DebugState::Prepared;
    debugFlagA_ = pData != nullptr;
    debugFlagB_ = constants_.viewMatrix[0] != 0.0f || constants_.viewMatrix[5] != 0.0f ||
                  constants_.viewMatrix[10] != 0.0f;
    debugFlagC_ = constants_.projMatrix[0] != 0.0f || constants_.projMatrix[5] != 0.0f ||
                  constants_.projMatrix[10] != 0.0f;
}

void ParticleRenderer::draw(IDeviceContext* pContext, size_t activeCount) {
    activeCount = std::min(activeCount, lastUploadedParticleCount_);
    debugCount_ = static_cast<qulonglong>(activeCount);
    debugUploaded_ = static_cast<qulonglong>(lastUploadedParticleCount_);
    if (!pContext || !prepared_ || !pImpl_->pPSO_ || !pImpl_->pSRB_ ||
        !pImpl_->pParticleBuffer_ || activeCount == 0) {
        debugState_ = DebugState::DrawSkipped;
        debugFlagA_ = pContext != nullptr;
        return;
    }
    
    if (frameCostStats_) {
        ++frameCostStats_->drawCalls;
    }
    const bool useIndirect =
        pImpl_->indirectDrawSupported_ && pImpl_->pIndirectArgsBuffer_ &&
        activeCount >= 64;
    if (pImpl_->gpuCullActive_) {
        DrawIndirectAttribs drawAttrs{
            pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE, 1, 0,
            sizeof(Uint32) * 4,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
        pContext->DrawIndirect(drawAttrs);
    } else if (useIndirect) {
        const Uint32 args[4] = {
            4u, static_cast<Uint32>(activeCount), 0u, 0u
        };
        pContext->UpdateBuffer(
            pImpl_->pIndirectArgsBuffer_, 0, sizeof(args), args,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) {
            ++frameCostStats_->bufferUpdates;
        }
        DrawIndirectAttribs drawAttrs{
            pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE, 1, 0,
            sizeof(args), RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
        pContext->DrawIndirect(drawAttrs);
    } else {
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 4;
        drawAttrs.NumInstances = static_cast<Uint32>(activeCount);
        drawAttrs.Flags = DRAW_FLAG_NONE;
        pContext->Draw(drawAttrs);
    }
    debugState_ = DebugState::Drawn;
    debugA_ = pImpl_->gpuCullActive_ ? 0 : (useIndirect ? 1 : 2);
    debugB_ = static_cast<qulonglong>(renderOptions_.blend);
    debugFlagA_ = renderOptions_.depthTest;
    debugFlagB_ = renderOptions_.depthWrite;
    debugFlagC_ = static_cast<int>(renderOptions_.billboard) != 0;
}

void ParticleRenderer::setProjectionMatrix(const float* matrix) {
    if (!matrix) {
        debugState_ = DebugState::MatrixUpdateSkippedProjection;
        qWarning() << "[ParticleRenderer] setProjectionMatrix() skipped: null matrix";
        return;
    }
    memcpy(constants_.projMatrix, matrix, sizeof(float) * 16);
    debugState_ = DebugState::MatrixUpdatedProjection;
}

void ParticleRenderer::setViewMatrix(const float* matrix) {
    if (!matrix) {
        debugState_ = DebugState::MatrixUpdateSkippedView;
        qWarning() << "[ParticleRenderer] setViewMatrix() skipped: null matrix";
        return;
    }
    memcpy(constants_.viewMatrix, matrix, sizeof(float) * 16);
    debugState_ = DebugState::MatrixUpdatedView;
}

void ParticleRenderer::setModelMatrix(const float* matrix) {
    if (!matrix) {
        debugState_ = DebugState::MatrixUpdateSkippedModel;
        qWarning() << "[ParticleRenderer] setModelMatrix() skipped: null matrix";
        return;
    }
    memcpy(constants_.modelMatrix, matrix, sizeof(float) * 16);
    debugState_ = DebugState::MatrixUpdatedModel;
}

QString ParticleRenderer::debugStateText() const {
    // Formats on demand only.  The old code built this string on every call and
    // stored it, so a steady particle frame paid for text nobody read.
    switch (debugState_) {
    case DebugState::Unknown:            return QStringLiteral("state=unknown");
    case DebugState::Constructed:        return QStringLiteral("state=constructed");
    case DebugState::Initialized:
        return QStringLiteral("state=initialize max=%1").arg(debugMax_);
    case DebugState::BuffersSkipped:
        return QStringLiteral("state=buffers-skipped device=%1 max=%2").arg(debugFlagA_ ? 1 : 0).arg(debugMax_);
    case DebugState::BuffersReady:
        return QStringLiteral("state=buffers-ready max=%1 particleBuffer=%2 constantBuffer=%3")
            .arg(debugMax_).arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0);
    case DebugState::PsoSkipped:
        return QStringLiteral("state=pso-skipped device=%1 max=%2 constantBuffer=%3")
            .arg(debugFlagA_ ? 1 : 0).arg(debugMax_).arg(debugFlagB_ ? 1 : 0);
    case DebugState::PsoFailed:
        return QStringLiteral("state=pso-failed max=%1 format=%2").arg(debugMax_).arg(debugFlagA_ ? 1 : 0);
    case DebugState::PsoMissingConstants:
        return QStringLiteral("state=pso-missing-constants max=%1 pso=ready").arg(debugMax_);
    case DebugState::PsoReady:
        return QStringLiteral("state=pso-ready max=%1 pso=%2 srb=%3 blend=%4 depthTest=%5 depthWrite=%6 format=rgba8-srgb")
            .arg(debugMax_).arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0)
            .arg(debugA_).arg(debugB_).arg(debugFlagC_ ? 1 : 0);
    case DebugState::UpdateEmpty:        return QStringLiteral("state=update-empty count=0");
    case DebugState::UpdateSkipped:
        return QStringLiteral("state=update-skipped ctx=%1 particleBuffer=%2 count=%3 uploaded=%4 max=%5")
            .arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0).arg(debugCount_).arg(debugUploaded_).arg(debugMax_);
    case DebugState::BufferUpdated:
        return QStringLiteral("state=buffer-updated count=%1 uploaded=%2 max=%3")
            .arg(debugCount_).arg(debugUploaded_).arg(debugMax_);
    case DebugState::PrepareSkippedContext:
        return QStringLiteral("state=prepare-skipped ctx=%1 pso=%2 srb=%3 constantBuffer=%4")
            .arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0).arg(debugFlagC_ ? 1 : 0).arg(1);
    case DebugState::PrepareSkippedConstantMap:
        return QStringLiteral("state=prepare-skipped constantBufferMap=0");
    case DebugState::PrepareSkippedBinding:
        return QStringLiteral("state=prepare-skipped particleVar=%1 particleSRV=%2")
            .arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0);
    case DebugState::PrepareWaitingPipeline:
        return QStringLiteral("state=prepare-waiting-pipeline workerDisabled=%1")
            .arg(debugFlagA_ ? 1 : 0);
    case DebugState::Prepared:
        return QStringLiteral("state=prepared pso=1 srb=1 const=%1 view=%2 proj=%3")
            .arg(debugFlagA_ ? 1 : 0).arg(debugFlagB_ ? 1 : 0).arg(debugFlagC_ ? 1 : 0);
    case DebugState::DrawSkipped:
        return QStringLiteral("state=draw-skipped ctx=%1 active=%2 uploaded=%3")
            .arg(debugFlagA_ ? 1 : 0).arg(debugCount_).arg(debugUploaded_);
    case DebugState::Drawn:
        return QStringLiteral("state=drawn active=%1 vertices=4 submission=%2 blend=%3 depthTest=%4 depthWrite=%5 billboard=%6")
            .arg(debugCount_)
            .arg(debugA_ == 0 ? QStringLiteral("gpu-cull-indirect")
                               : (debugA_ == 1 ? QStringLiteral("indirect")
                                                : QStringLiteral("direct")))
            .arg(debugB_)
            .arg(debugFlagA_ ? 1 : 0)
            .arg(debugFlagB_ ? 1 : 0)
            .arg(debugFlagC_ ? 1 : 0);
    case DebugState::MatrixUpdateSkippedProjection:
        return QStringLiteral("state=matrix-update-skipped projection=0");
    case DebugState::MatrixUpdateSkippedView:
        return QStringLiteral("state=matrix-update-skipped view=0");
    case DebugState::MatrixUpdateSkippedModel:
        return QStringLiteral("state=matrix-update-skipped model=0");
    case DebugState::MatrixUpdatedView:
        return QStringLiteral("state=matrix-updated view=%1 proj=%2")
            .arg(1).arg(debugFlagA_ ? 1 : 0);
    case DebugState::MatrixUpdatedProjection:
        return QStringLiteral("state=matrix-updated view=%1 proj=%2")
            .arg(debugFlagB_ ? 1 : 0).arg(1);
    case DebugState::MatrixUpdatedModel:
        return QStringLiteral("state=matrix-updated model=1");
    }
    return QStringLiteral("state=unknown");
}

} // namespace ArtifactCore
