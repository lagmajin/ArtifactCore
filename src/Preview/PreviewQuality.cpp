module;

#include <QString>
#include <QSize>

module Preview.Quality;

import std;

namespace ArtifactCore {

 class PreviewQuality::Impl {
 public:
  Scale scale_ = Scale::Full;
  RenderMode renderMode_ = RenderMode::Normal;
  AntiAliasing antiAliasing_ = AntiAliasing::None;
  TextureQuality textureQuality_ = TextureQuality::High;
  int maxFrameRate_ = 0;
  bool motionBlurEnabled_ = true;
  bool depthOfFieldEnabled_ = true;
  bool shadowsEnabled_ = true;
  QString currentPreset_ = "Default";

  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };
	
 PreviewQuality::PreviewQuality():impl_(new Impl)
 {

 }

 PreviewQuality::PreviewQuality(const PreviewQuality& other) :impl_(new Impl(*other.impl_))
 {

 }

 PreviewQuality::PreviewQuality(Scale scale) :impl_(new Impl)
 {
  impl_->scale_ = scale;
 }

 PreviewQuality::~PreviewQuality()
 {
  delete impl_;
 }

 PreviewQuality::Scale PreviewQuality::quality() const
 {
  return impl_->scale_;
 }

 void PreviewQuality::setQuality(Scale scale)
 {
  impl_->scale_ = scale;
 }

 PreviewQuality::RenderMode PreviewQuality::renderMode() const
 {
  return impl_->renderMode_;
 }

 void PreviewQuality::setRenderMode(RenderMode mode)
 {
  impl_->renderMode_ = mode;
 }

 void PreviewQuality::setMaxFrameRate(int fps)
 {
  impl_->maxFrameRate_ = fps;
 }

 int PreviewQuality::maxFrameRate() const
 {
  return impl_->maxFrameRate_;
 }

 bool PreviewQuality::isFrameRateLimited() const
 {
  return impl_->maxFrameRate_ > 0;
 }

 void PreviewQuality::setAntiAliasing(AntiAliasing aa)
 {
  impl_->antiAliasing_ = aa;
 }

 PreviewQuality::AntiAliasing PreviewQuality::antiAliasing() const
 {
  return impl_->antiAliasing_;
 }

 void PreviewQuality::setTextureQuality(TextureQuality quality)
 {
  impl_->textureQuality_ = quality;
 }

 PreviewQuality::TextureQuality PreviewQuality::textureQuality() const
 {
  return impl_->textureQuality_;
 }

 void PreviewQuality::setEnableMotionBlur(bool enable)
 {
  impl_->motionBlurEnabled_ = enable;
 }

 bool PreviewQuality::isMotionBlurEnabled() const
 {
  return impl_->motionBlurEnabled_;
 }

 void PreviewQuality::setEnableDepthOfField(bool enable)
 {
  impl_->depthOfFieldEnabled_ = enable;
 }

 bool PreviewQuality::isDepthOfFieldEnabled() const
 {
  return impl_->depthOfFieldEnabled_;
 }

 void PreviewQuality::setEnableShadows(bool enable)
 {
  impl_->shadowsEnabled_ = enable;
 }

 bool PreviewQuality::areShadowsEnabled() const
 {
  return impl_->shadowsEnabled_;
 }

 float PreviewQuality::scaleMultiplier() const
 {
  switch (impl_->scale_) {
   case Scale::Full: return 1.0f;
   case Scale::Half: return 0.5f;
   case Scale::Quarter: return 0.25f;
   case Scale::Eighth: return 0.125f;
   case Scale::Auto: return 0.5f;
   default: return 1.0f;
  }
 }

 void PreviewQuality::applyPreset(const QString& presetName)
 {
  impl_->currentPreset_ = presetName;
  
  if (presetName == "High") {
   impl_->scale_ = Scale::Full;
   impl_->renderMode_ = RenderMode::Normal;
   impl_->antiAliasing_ = AntiAliasing::MSAA_4x;
   impl_->textureQuality_ = TextureQuality::High;
   impl_->motionBlurEnabled_ = true;
   impl_->depthOfFieldEnabled_ = true;
   impl_->shadowsEnabled_ = true;
  } else if (presetName == "Medium") {
   impl_->scale_ = Scale::Half;
   impl_->renderMode_ = RenderMode::Normal;
   impl_->antiAliasing_ = AntiAliasing::FXAA;
   impl_->textureQuality_ = TextureQuality::Medium;
   impl_->motionBlurEnabled_ = true;
   impl_->depthOfFieldEnabled_ = false;
   impl_->shadowsEnabled_ = true;
  } else if (presetName == "Low") {
   impl_->scale_ = Scale::Quarter;
   impl_->renderMode_ = RenderMode::Fast;
   impl_->antiAliasing_ = AntiAliasing::None;
   impl_->textureQuality_ = TextureQuality::Low;
   impl_->motionBlurEnabled_ = false;
   impl_->depthOfFieldEnabled_ = false;
   impl_->shadowsEnabled_ = false;
  } else if (presetName == "Draft") {
   impl_->scale_ = Scale::Eighth;
   impl_->renderMode_ = RenderMode::Draft;
   impl_->antiAliasing_ = AntiAliasing::None;
   impl_->textureQuality_ = TextureQuality::Low;
   impl_->motionBlurEnabled_ = false;
   impl_->depthOfFieldEnabled_ = false;
   impl_->shadowsEnabled_ = false;
  } else {
   impl_->scale_ = Scale::Full;
   impl_->renderMode_ = RenderMode::Normal;
   impl_->antiAliasing_ = AntiAliasing::None;
   impl_->textureQuality_ = TextureQuality::High;
  }
 }

 QString PreviewQuality::currentPresetName() const
 {
  return impl_->currentPreset_;
 }

 bool PreviewQuality::isHighQuality() const
 {
  return impl_->scale_ == Scale::Full && impl_->renderMode_ == RenderMode::Normal;
 }

 bool PreviewQuality::isLowQuality() const
 {
  return impl_->scale_ == Scale::Quarter || impl_->scale_ == Scale::Eighth || 
         impl_->renderMode_ == RenderMode::Draft;
 }

 QSize PreviewQuality::calculatePreviewSize(const QSize& originalSize) const
 {
  float multiplier = scaleMultiplier();
  return QSize(
   static_cast<int>(originalSize.width() * multiplier),
   static_cast<int>(originalSize.height() * multiplier)
  );
 }

 bool PreviewQuality::operator==(const PreviewQuality& o) const
 {
  return impl_->scale_ == o.impl_->scale_ &&
         impl_->renderMode_ == o.impl_->renderMode_ &&
         impl_->antiAliasing_ == o.impl_->antiAliasing_ &&
         impl_->textureQuality_ == o.impl_->textureQuality_ &&
         impl_->maxFrameRate_ == o.impl_->maxFrameRate_;
 }

 bool PreviewQuality::operator!=(const PreviewQuality& o) const
 {
  return !(*this == o);
 }

};
