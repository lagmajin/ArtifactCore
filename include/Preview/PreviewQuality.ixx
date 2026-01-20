module ;

#include "../Define/DllExportMacro.hpp"
#include <QString>
#include <QSize>

export module Preview.Quality;

export namespace ArtifactCore {

 class LIBRARY_DLL_API PreviewQuality final{
 private:
  class Impl;
  Impl* impl_;
 public:
  enum class Scale {
   Full,
   Half,
   Quarter,
   Eighth,
   Auto
  };

  enum class RenderMode {
   Normal,      // 通常レンダリング
   Fast,        // エフェクトOFF / 簡易カラー処理
   Draft        // 最低品質
  };

  enum class AntiAliasing {
   None,
   FXAA,
   MSAA_2x,
   MSAA_4x,
   MSAA_8x
  };

  enum class TextureQuality {
   Low,
   Medium,
   High,
   Original
  };

  PreviewQuality();
  explicit PreviewQuality(Scale scale);
  PreviewQuality(const PreviewQuality& other);
  ~PreviewQuality();

  // Scale
  Scale quality() const;
  void setQuality(Scale scale);

  // RenderMode
  RenderMode renderMode() const;
  void setRenderMode(RenderMode mode);

  // Frame rate limiting
  void setMaxFrameRate(int fps);
  int maxFrameRate() const;
  bool isFrameRateLimited() const;

  // Anti-aliasing
  void setAntiAliasing(AntiAliasing aa);
  AntiAliasing antiAliasing() const;

  // Texture quality
  void setTextureQuality(TextureQuality quality);
  TextureQuality textureQuality() const;

  // Performance hints
  void setEnableMotionBlur(bool enable);
  bool isMotionBlurEnabled() const;
  void setEnableDepthOfField(bool enable);
  bool isDepthOfFieldEnabled() const;
  void setEnableShadows(bool enable);
  bool areShadowsEnabled() const;

  // Utility
  float scaleMultiplier() const;
  void applyPreset(const QString& presetName);
  QString currentPresetName() const;
  bool isHighQuality() const;
  bool isLowQuality() const;

  // Resolution calculation
  QSize calculatePreviewSize(const QSize& originalSize) const;
 	
  bool operator==(const PreviewQuality& o) const;
  bool operator!=(const PreviewQuality& o) const;
 };

};