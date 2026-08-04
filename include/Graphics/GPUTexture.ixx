module;
#include <utility>
export module Graphics.Texture;

export namespace ArtifactCore {

 enum class GPUTextureFormat {
  Unknown,
  RGBA8_UNorm,
  RGBA8_UNorm_SRGB,
  RGBA16_Float,
  RGBA32_Float,
  R32_Float
 };

 class GPUTexture {
 private:
  class Impl;
  Impl* impl_;
 public:
  GPUTexture();
  ~GPUTexture();

  GPUTexture(const GPUTexture&) = delete;
  GPUTexture& operator=(const GPUTexture&) = delete;

  GPUTexture(GPUTexture&& other) noexcept;
  GPUTexture& operator=(GPUTexture&& other) noexcept;

  bool Create(int width, int height,
              GPUTextureFormat format = GPUTextureFormat::RGBA8_UNorm,
              int mipLevels = 1);
  void Reset();
  bool IsValid() const;

  int GetWidth() const;
  int GetHeight() const;
  GPUTextureFormat GetFormat() const;
  int GetMipLevels() const;
 };

};
