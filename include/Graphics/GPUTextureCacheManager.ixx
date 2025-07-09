module;

#include <QUuid>
#include <QObject>
export module Graphics.ImageCache;


export namespace ArtifactCore {

 class GPUTextureManager:public QObject {
 private:

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