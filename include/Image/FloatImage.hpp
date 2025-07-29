#pragma once

#include <cstdint>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QSize>

//#include "../Color/FloatRGBA.hpp"

import Color.Float;


namespace ArtifactCore {

 enum eFloatImageFormat {
  FORMAT_RGBA,
  FORMAT_RGB,
  FORMAT_XYZ,

 };

 class FloatImagePrivate;

 class FloatImage {
 private:

 public:
  FloatImage();
  ~FloatImage();
  int width() const;
  int height() const;
  void fill(const FloatColor& color);
  void resize();
  void resize(int width, int height);

  FloatImage clone() const;
 };

}