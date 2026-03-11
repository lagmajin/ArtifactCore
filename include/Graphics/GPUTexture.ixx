module;
export module Graphics.Texture;

export namespace ArtifactCore {

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

  int GetWidth() const;
  int GetHeight() const;
 };

};