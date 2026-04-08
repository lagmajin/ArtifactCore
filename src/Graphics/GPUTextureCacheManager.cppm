module;
#include <utility>
#include <QList>
#include <QUuid>

module Graphics.ImageCache;

namespace ArtifactCore {

 class GPUTextureManager::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
 };

 GPUTextureManager::GPUTextureManager() : impl_(new Impl())
 {
 }

 GPUTextureManager::~GPUTextureManager()
 {
  delete impl_;
 }

 void GPUTextureManager::clear()
 {
 }

};
