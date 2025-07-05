module;

export module Media.MediaFrame;

import std;


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