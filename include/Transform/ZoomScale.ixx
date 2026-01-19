module;

#include "../Define/DllExportMacro.hpp"

export module Core.Scale.Zoom;

import std;


export namespace ArtifactCore {

 class LIBRARY_DLL_API ZoomScale2D {
 private:
  class Impl;
  Impl* impl_;
 public:
  ZoomScale2D();
  ZoomScale2D(float initialScale);
  ZoomScale2D(const ZoomScale2D& scale);
  ~ZoomScale2D();

  // Basic operations
  void ZoomIn(float factor = 1.1f);
  void ZoomOut(float factor = 1.1f);
  void setScale(float scale);
  float scale() const;

  // Range control
  void setMinScale(float minScale);
  void setMaxScale(float maxScale);
  float minScale() const;
  float maxScale() const;

  // Utility
  void reset();
  bool isDefault() const;
  std::string toString() const;

  // Operators
  ZoomScale2D& operator+=(float delta);
  ZoomScale2D& operator-=(float delta);
  ZoomScale2D& operator*=(float factor);
  ZoomScale2D& operator/=(float factor);
  ZoomScale2D operator*(float factor) const;
  ZoomScale2D operator/(float factor) const;

  // Comparison
  bool operator==(const ZoomScale2D& other) const;
  bool operator!=(const ZoomScale2D& other) const;

  ZoomScale2D& operator=(const ZoomScale2D& other);
  ZoomScale2D& operator=(ZoomScale2D&& other) noexcept;
 };








};