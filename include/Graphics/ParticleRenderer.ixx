module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
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
     * @brief CPUプールのデータをGPU構造化バッファへアップロード
     */
    void updateBuffer(const ParticleRenderData& data);

    /**
     * @brief 描画準備
     * パイプライン状態の設定とリソースのコミットを行います。
     */
    void prepare(IDeviceContext* pContext);

    /**
     * @brief 最終的な描画命令の発行
     */
    void draw(IDeviceContext* pContext, size_t activeCount);

    // 設定
    void setProjectionMatrix(const float* matrix); // float[16]
    void setViewMatrix(const float* matrix);       // float[16]

private:
    GpuContext& context_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxParticles_ = 0;

    struct ShaderConstants {
        float viewMatrix[16];
        float projMatrix[16];
        float deltaTime;
        float padding[3];
    };
    ShaderConstants constants_;
    ArtifactCore::RenderCostStats* frameCostStats_ = nullptr;

    void createPSO();
    void createBuffers();
};

} // namespace ArtifactCore
