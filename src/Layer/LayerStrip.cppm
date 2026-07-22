module;
#include <utility>
#include <cstdint>

module Layer.LayerStrip;

namespace ArtifactCore {

 class LayerStrip::Impl {
 public:
  Impl() : startFrame_(0), endFrame_(0) {}
  Impl(int32_t start, int32_t end) : startFrame_(start), endFrame_(end) {}
  ~Impl() {}

  int32_t startFrame_ = 0;
  int32_t endFrame_ = 0;
 };

 LayerStrip::LayerStrip() : impl_(new Impl())
 {
 }

 LayerStrip::LayerStrip(int32_t startFrame, int32_t endFrame)
  : impl_(new Impl(startFrame, endFrame))
 {
 }

 LayerStrip::~LayerStrip()
 {
  delete impl_;
 }

 void LayerStrip::SetStartFrame(int32_t frame)
 {
  impl_->startFrame_ = frame;
 }

 void LayerStrip::SetEndFrame(int32_t frame)
 {
  impl_->endFrame_ = frame;
 }

 void LayerStrip::SetRange(int32_t startFrame, int32_t endFrame)
 {
  impl_->startFrame_ = startFrame;
  impl_->endFrame_ = endFrame;
 }

 int32_t LayerStrip::StartFrame() const
 {
  return impl_->startFrame_;
 }

 int32_t LayerStrip::EndFrame() const
 {
  return impl_->endFrame_;
 }

 int32_t LayerStrip::Duration() const
 {
  if (impl_->endFrame_ < impl_->startFrame_) return 0;
  return impl_->endFrame_ - impl_->startFrame_;
 }

 bool LayerStrip::IsValid() const
 {
  return impl_->startFrame_ <= impl_->endFrame_;
 }

 bool LayerStrip::ContainsFrame(int32_t frame) const
 {
  return frame >= impl_->startFrame_ && frame <= impl_->endFrame_;
 }

 void LayerStrip::Shift(int32_t offset)
 {
  impl_->startFrame_ += offset;
  impl_->endFrame_ += offset;
 }

};