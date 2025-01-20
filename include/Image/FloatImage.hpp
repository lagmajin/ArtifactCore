#pragma once

#include <cstdint>
#include <memory>


#include <QtCore/QSize>

#include "../Color/FloatColor.hpp"


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

 };

}