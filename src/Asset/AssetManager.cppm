module;
#include <QFileSystemWatcher>

module Asset.Manager;

namespace ArtifactCore {

 class AssetManager::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
 };

 AssetManager::AssetManager() : impl_(new Impl())
 {
 }

 AssetManager::~AssetManager()
 {
  delete impl_;
 }

};
