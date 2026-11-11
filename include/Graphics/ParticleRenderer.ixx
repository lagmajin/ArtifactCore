module;
#include <QString>
#include <cstdint>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)
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
export module Graphics.ParticleRenderer;

import Graphics.ParticleData;
import Graphics.GPUcomputeContext;
import Frame.Debug;


export namespace ArtifactCore {

using namespace Diligent;

/**
 * @brief Diligent / DX12 用のパーティクルレンダリング基盤
 * インスタンス描画と構造化バッファを用いた高速な描画を管理します。
 */
class LIBRARY_DLL_API ParticleRenderer {
public:
    ParticleRenderer(GpuContext& context);
    ~ParticleRenderer();

    /**
     * @brief レンダリング用リソース（PSO, Buffer）の初期化
     */
    void initialize(size_t maxParticles);
    void setFrameCostStats(ArtifactCore::RenderCostStats* stats);

    /**
     * @brief CPUプールのデータを指定contextのcommand streamへアップロード
     */
    void updateBuffer(IDeviceContext* pContext, const ParticleRenderData& data);
    void setRenderOptions(const ParticleRenderOptions& options);
    size_t lastUploadedParticleCount() const;

    /**
     * @brief 単一画像アトラスを登録し、安定したテクスチャ id を返す
     *
     * 画素データは呼び出し中だけ読み取る。0 は登録失敗（デバイス無し・
     * 空画像・確保失敗）を表し、呼び出し側は手続き描画へフォールバックする。
     * GUI スレッドから呼ぶこと。
     */
    ParticleTextureId createTexture(const ParticleTextureUpload& upload);

    /// 登録済みテクスチャを破棄する。id を保持したままの描画パケットは
    /// 手続き描画へフォールバックする。
    void releaseTexture(ParticleTextureId id);

    /// 登録済みテクスチャの SRV を返す（所有権は移動しない）。未登録なら null。
    ITextureView* textureView(ParticleTextureId id) const;

    /// このレンダラーのテクスチャ登録世代。レンダラー再生成時に変化する。
    std::uint64_t textureGeneration() const { return textureGeneration_; }

    /// パケットが保持するテクスチャを現在の描画に適用する。
    /// updateBuffer() の後、prepare() の前に呼ぶこと。
    void setCurrentTexture(ITextureView* textureView);

    /**
     * @brief 描画準備
     * パイプライン状態の設定とリソースのコミットを行います。
     */
    void prepare(IDeviceContext* pContext);

    // Ensure a requested graphics pipeline exists before a queued draw is
    // accepted. Offline export uses this so an async PSO miss can fall back to
    // the layer's software rasterizer instead of silently dropping a frame.
    bool ensureGraphicsPipeline(const ParticleRenderOptions& options);

    /**
     * @brief 最終的な描画命令の発行
     */
    void draw(IDeviceContext* pContext, size_t activeCount);

    // 設定
    void setProjectionMatrix(const float* matrix); // float[16]
    void setViewMatrix(const float* matrix);       // float[16]
    void setModelMatrix(const float* matrix);      // float[16], row-major

    /// Lifecycle state of the renderer.  This is the authoritative value: the
    /// human readable form is derived from it on demand, so a frame that never
    /// reports a problem never pays for formatting a debug string.
    enum class DebugState : std::uint8_t {
        Unknown = 0,
        Constructed,
        Initialized,
        BuffersSkipped,
        BuffersReady,
        PsoSkipped,
        PsoFailed,
        PsoMissingConstants,
        PsoReady,
        UpdateEmpty,
        UpdateSkipped,
        BufferUpdated,
        PrepareSkippedContext,
        PrepareSkippedConstantMap,
        PrepareSkippedBinding,
        PrepareWaitingPipeline,
        Prepared,
        DrawSkipped,
        Drawn,
        MatrixUpdateSkippedProjection,
        MatrixUpdateSkippedView,
        MatrixUpdateSkippedModel,
        MatrixUpdatedView,
        MatrixUpdatedProjection,
        MatrixUpdatedModel,
    };

    /// Cheap, allocation-free state query.  Prefer this over debugStateText()
    /// for control flow; it never builds a string.
    DebugState debugState() const { return debugState_; }
    bool isPrepared() const { return debugState_ == DebugState::Prepared; }

    /// Human readable state, including the numeric details the old debugState_
    /// string carried.  Formats on every call, so call it only when the value
    /// is actually going to be reported.
    QString debugStateText() const;

    // Enqueues background builds for the common pipelines (default additive
    // + alpha, plus the cull pipeline) so the first particle draw finds them
    // ready. Safe to call repeatedly; cached/failed builds are skipped.
    void prewarmCommonPipelines();

private:
    GpuContext& context_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxParticles_ = 0;
    size_t lastUploadedParticleCount_ = 0;
    ParticleRenderOptions renderOptions_;

    struct ShaderConstants {
        float modelMatrix[16];
        float viewMatrix[16];
        float projMatrix[16];
        float deltaTime;
        int billboardMode;
        int falloffMode;
        float padding;
        int textureMode;
        // [0] = 1/atlasWidth, [1] = 1/atlasHeight, [2] = reserved.
        float texturePadding[3];
    };
    ShaderConstants constants_{};
    ArtifactCore::RenderCostStats* frameCostStats_ = nullptr;
    // Texture atlas currently applied by setCurrentTexture(). Owned by the
    // registry or by a queued packet; prepare() only reads the pointer.
    ITextureView* currentTextureView_ = nullptr;
    std::uint64_t textureGeneration_ = 0;
    // Hot-path state as a value.  The numeric fields feed debugStateText() only,
    // so a frame that reports nothing never formats a string.
    DebugState debugState_ = DebugState::Unknown;
    quint64 debugCount_ = 0;
    quint64 debugUploaded_ = 0;
    quint64 debugMax_ = 0;
    quint64 debugA_ = 0;
    quint64 debugB_ = 0;
    bool debugFlagA_ = false;
    bool debugFlagB_ = false;
    bool debugFlagC_ = false;

    /// True once prepare() has committed this frame's constants.  Kept separate
    /// from debugState_ so callers can ask without inspecting the enum.
    bool prepared_ = false;

    void createBuffers();
    // Graphics pipelines compile on a background thread (see the async worker
    // in the implementation): layer addition and first draw never block on
    // dxc. The CPU fallback covers frames until the build lands.
    void pumpAsyncResults();
    bool useCachedGraphicsPso();
    void requestAsyncGraphics();
    void requestAsyncBuild(const ParticleRenderOptions& options);
    void requestAsyncCull();
    void buildGraphicsSync();
    void markPsoReady();
    void asyncWorkerMain();
    // Builds the GPU visibility-cull compute pipeline on first use (legacy
    // synchronous path, used only when the async worker is unavailable).
    // Returns true once it is ready; safe to call every frame.
    bool ensureCullPipeline();
};

} // namespace ArtifactCore
