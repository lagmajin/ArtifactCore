module;

#include "../Define/DllExportMacro.hpp"

export module Core.Scale.Zoom;


export namespace ArtifactCore {

 class LIBRARY_DLL_API ZoomScale2D {
 private:
  class Impl;
  Impl* impl_;
 public:
  ZoomScale2D();
  ZoomScale2D(const ZoomScale2D& scale);
  ~ZoomScale2D();
  void ZoomIn(float factor);
  void ZoomOut(float factor);
  float scale() const;
  ZoomScale2D& operator+=(float delta);

  ZoomScale2D& operator=(const ZoomScale2D& other);
  ZoomScale2D& operator=(ZoomScale2D&& other) noexcept;
 };








};