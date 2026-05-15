module;
#include "../../Define/DllExportMacro.hpp"
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>


export module Graphics.Compute.Histogram;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore {

using namespace Diligent;

class LIBRARY_DLL_API HistogramComputer {
public:
  explicit HistogramComputer(GpuContext &context);
  ~HistogramComputer();

  void initialize();

  /**
   * @brief テクスチャ全体の輝度ヒストグラムを計算
   * @param inputTexture 入力テクスチャ (RGBA8_UNORM)
   * @param outputHistogram 出力バッファ: uint32_t[256]
   */
  void computeLuminance(IDeviceContext *pContext, ITextureView *inputTexture,
                        IBuffer *outputHistogram);

  /**
   * @brief 指定した矩形領域内のヒストグラムを計算
   */
  void computeLuminanceRegion(IDeviceContext *pContext,
                              ITextureView *inputTexture, uint32_t x,
                              uint32_t y, uint32_t width, uint32_t height,
                              IBuffer *outputHistogram);

  /**
   * @brief RGB 各チャンネル別々のヒストグラムを計算
   * @param outputHistogram 出力バッファ: uint32_t[256 * 3]
   */
  void computeRGB(IDeviceContext *pContext, ITextureView *inputTexture,
                  IBuffer *outputHistogram);

  bool ready() const;

private:
  GpuContext &context_;
  ComputeExecutor executorLuminance_;
  ComputeExecutor executorRGB_;

  RefCntAutoPtr<IBuffer> pHistogramTempBuffer_;

  void createPipelines();
  void createBuffers();

  static constexpr uint32_t THREAD_GROUP_SIZE = 256;
  static constexpr uint32_t BIN_COUNT = 256;
};

} // namespace ArtifactCore
