#pragma once

#include <memory>


namespace ArtifactCore {

 class MediaFramePrivate;

 class MediaFrame {
 private:
  std::unique_ptr<MediaFramePrivate> const pImpl_;
 public:
  MediaFrame();
  ~MediaFrame();
 };










};