module;
#include <utility>
#include <algorithm>

module Preview.Settings;

namespace ArtifactCore
{
 class PreviewSettings::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  bool multiFrameRendering = false;
  int frameSkip = 0;
  int targetFps = 0;
  float resolutionScale = 1.0f;
  bool enableWireframe = false;
  bool enableGrid = false;
  bool enableSafeAreas = false;
 };

 PreviewSettings::Impl::Impl()
 {

 }

 PreviewSettings::Impl::~Impl()
 {

 }

 PreviewSettings::PreviewSettings() : impl_(new Impl())
 {

 }

 PreviewSettings::PreviewSettings(const PreviewSettings& other) : impl_(new Impl())
 {
  impl_->multiFrameRendering = other.impl_->multiFrameRendering;
  impl_->frameSkip = other.impl_->frameSkip;
  impl_->targetFps = other.impl_->targetFps;
  impl_->resolutionScale = other.impl_->resolutionScale;
  impl_->enableWireframe = other.impl_->enableWireframe;
  impl_->enableGrid = other.impl_->enableGrid;
  impl_->enableSafeAreas = other.impl_->enableSafeAreas;
 }

 PreviewSettings::~PreviewSettings()
 {
  delete impl_;
 }

 bool PreviewSettings::multiFrameRendering() const
 {
  return impl_->multiFrameRendering;
 }

 void PreviewSettings::setMultiFrameRendering(bool enable)
 {
  impl_->multiFrameRendering = enable;
 }

 int PreviewSettings::frameSkip() const
 {
  return impl_->frameSkip;
 }

 void PreviewSettings::setFrameSkip(int skip)
 {
  impl_->frameSkip = std::max(0, skip);
 }

 int PreviewSettings::targetFps() const
 {
  return impl_->targetFps;
 }

 void PreviewSettings::setTargetFps(int fps)
 {
  impl_->targetFps = std::max(0, fps);
 }

 float PreviewSettings::resolutionScale() const
 {
  return impl_->resolutionScale;
 }

 void PreviewSettings::setResolutionScale(float scale)
 {
  impl_->resolutionScale = std::clamp(scale, 0.125f, 1.0f);
 }

 bool PreviewSettings::enableWireframe() const
 {
  return impl_->enableWireframe;
 }

 void PreviewSettings::setEnableWireframe(bool enable)
 {
  impl_->enableWireframe = enable;
 }

 bool PreviewSettings::enableGrid() const
 {
  return impl_->enableGrid;
 }

 void PreviewSettings::setEnableGrid(bool enable)
 {
  impl_->enableGrid = enable;
 }

 bool PreviewSettings::enableSafeAreas() const
 {
  return impl_->enableSafeAreas;
 }

 void PreviewSettings::setEnableSafeAreas(bool enable)
 {
  impl_->enableSafeAreas = enable;
 }

 void PreviewSettings::reset()
 {
  impl_->multiFrameRendering = false;
  impl_->frameSkip = 0;
  impl_->targetFps = 0;
  impl_->resolutionScale = 1.0f;
  impl_->enableWireframe = false;
  impl_->enableGrid = false;
  impl_->enableSafeAreas = false;
 }

};