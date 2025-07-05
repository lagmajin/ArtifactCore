module;

#include <QUuid>
export module Graphics.ImageCache;


export namespace ArtifactCore {

 class GPUTextureManager {
 private:

 public:
  GPUTextureManager();
  
  ~GPUTextureManager();
  GPUTextureManager(const GPUTextureManager&) = delete;
  GPUTextureManager& operator=(const GPUTextureManager&) = delete;

  QUuid createTexture();

  void clear();
 };









};