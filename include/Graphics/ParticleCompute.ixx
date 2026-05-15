module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)
#include "../Define/DllExportMacro.hpp"
#include <vector>

export module Graphics.ParticleCompute;

import Particle;
import Graphics.GPUcomputeContext;
import Graphics.Compute;

export namespace ArtifactCore {

using namespace Diligent;

/**
 * @brief GPU Compute によるパーティクル更新の基盤クラス
 * 計算ロジックをまるごとGPUで行い、CPU-GPU間の転送を最小化します。
 */
class LIBRARY_DLL_API ParticleCompute {
public:
    ParticleCompute(GpuContext& context);
    ~ParticleCompute();

    /**
     * @brief コンピュートリソースの初期化
     */
    void initialize(size_t maxParticles);

    /**
     * @brief シミュレーションの実行 (Compute Shader Dispatch)
     */
    void dispatch(IDeviceContext* pContext, float dt);

    /**
     * @brief パーティクルデータをバッファへ初期流し込み (インジェクション)
     */
    void uploadParticles(const std::vector<Particle>& particles, size_t count);

    /**
     * @brief オーディオデータの供給 (スペクトラムデータ)
     */
    void setAudioData(const std::vector<float>& spectrum);

    /**
     * @brief 描画用レンダラーにバッファを渡すために取得
     */
    IBuffer* getParticleBuffer();

private:
    GpuContext& context_;
    ComputeExecutor                       executor_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxParticles_ = 0;

    struct SimulationConstants {
        float deltaTime;
        float time;
        uint32_t maxParticles;
        float noiseStrength;
        float audioIntensity; // 全体的な音の強さ
        float _padding[3];
    };
    SimulationConstants constants_;

    void createPSO();
    void createBuffers();
};

} // namespace ArtifactCore
