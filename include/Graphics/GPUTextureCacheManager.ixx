module;
#include <utility>
#include <QUuid>
#include <QObject>

export module Graphics.ImageCache;

export namespace ArtifactCore {

 class GPUTextureManager:public QObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  GPUTextureManager();
  
  ~GPUTextureManager();
  GPUTextureManager(const GPUTextureManager&) = delete;
  GPUTextureManager& operator=(const GPUTextureManager&) = delete;

  QUuid createTexture();
  void removeTexture();

  void clear();
 };









};
