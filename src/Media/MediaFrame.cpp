#include "../../include/Media/MediaFrame.hpp"





namespace ArtifactCore {

 class MediaFramePrivate {
 private:

 public:
  MediaFramePrivate();
  ~MediaFramePrivate();
 };

 MediaFramePrivate::MediaFramePrivate()
 {

 }

 MediaFramePrivate::~MediaFramePrivate()
 {

 }

 MediaFrame::MediaFrame() :pImpl_(std::make_unique<MediaFramePrivate>())
 {

 }

 MediaFrame::~MediaFrame()
 {

 }

}