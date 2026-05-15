module;
#include <utility>

// RefCntAutoPtr.hpp intentionally NOT included here (MSVC 14.51 C1116 workaround)
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../../ArtifactCore/include/Define/DllExportMacro.hpp"
#include <memory>

export module Graphics.GPUCompressionPipeline;

import Graphics.Compute;
import Graphics.GPUcomputeContext;

export namespace ArtifactCore
{
  using namespace Diligent;

  struct CompressionParams {
    uint32_t blockSize = 4096;  // 4KB blocks
    uint32_t numBlocks = 0;
    uint32_t _pad0 = 0;
    uint32_t _pad1 = 0;
  };

  class LIBRARY_DLL_API GPUCompressionPipeline
  {
  public:
    explicit GPUCompressionPipeline(GpuContext& context);
    ~GPUCompressionPipeline();

    bool initialize();

    // Compress texture data to compressed buffer
    bool compress(
      IDeviceContext* ctx,
      ITextureView* srcSRV,      // RGBA16_FLOAT texture to compress
      IBuffer* compressedOutput, // Output compressed data buffer
      Uint32 width,
      Uint32 height
    );

    // Decompress compressed data back to texture
    bool decompress(
      IDeviceContext* ctx,
      IBuffer* compressedInput,  // Input compressed data buffer
      ITextureView* dstUAV,      // RGBA16_FLOAT texture output
      Uint32 width,
      Uint32 height
    );

    bool ready() const;

  private:
    bool createConstantBuffer();
    bool createExecutors();

    struct CompressionExecutor {
      std::unique_ptr<ComputeExecutor> executor;
    };

    GpuContext& context_;
    class Impl;
    Impl* pImpl_ = nullptr;
    CompressionExecutor compressExecutor_;
    CompressionExecutor decompressExecutor_;
    CompressionParams currentParams_{};
  };
}
