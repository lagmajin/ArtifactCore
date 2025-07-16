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
  int GetWidth() const;
  int GetHeight() const;
 };

};