module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
export module Preview.Settings;

export namespace ArtifactCore
{
 
 class LIBRARY_DLL_API PreviewSettings
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  PreviewSettings();
  PreviewSettings(const PreviewSettings& other);
  ~PreviewSettings();

  // Multi-frame rendering
  bool multiFrameRendering() const;
  void setMultiFrameRendering(bool enable);

  // Frame skip (render every Nth frame; 0 = disabled)
  int frameSkip() const;
  void setFrameSkip(int skip);

  // Target FPS for preview playback (0 = unlimited)
  int targetFps() const;
  void setTargetFps(int fps);

  // Resolution scale factor (1.0 = full, 0.5 = half, 0.25 = quarter)
  float resolutionScale() const;
  void setResolutionScale(float scale);

  // Overlay toggles
  bool enableWireframe() const;
  void setEnableWireframe(bool enable);

  bool enableGrid() const;
  void setEnableGrid(bool enable);

  bool enableSafeAreas() const;
  void setEnableSafeAreas(bool enable);

  // Reset to defaults
  void reset();
 };

}