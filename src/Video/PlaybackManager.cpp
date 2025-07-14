module;
#include <wobjectimpl.h>
module Video.PlaybackManager;

namespace ArtifactCore {

 W_OBJECT_IMPL(PlaybackManager)

 class PlaybackManager::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 PlaybackManager::Impl::Impl()
 {

 }

 PlaybackManager::Impl::~Impl()
 {

 }

 PlaybackManager::PlaybackManager(QObject* parent /*= nullptr*/)
 {

 }

 PlaybackManager::~PlaybackManager()
 {

 }

 bool PlaybackManager::open(const QString& filePath)
 {

  return true;
 }

 void PlaybackManager::play()
 {

 }

};