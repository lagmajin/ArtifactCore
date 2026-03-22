
//#include <Halide.h>

#include "../../include/Image/FloatImage.hpp"





namespace ArtifactCore {
 
 class FloatImagePrivate {
 private:

 public:
  FloatImagePrivate();
  ~FloatImagePrivate();
  int width() const;
  int height() const;
  void resize();
  void resize(int width, int height);
 };

 FloatImagePrivate::FloatImagePrivate()
 {

 }

 FloatImagePrivate::~FloatImagePrivate()
 {

 }

 int FloatImagePrivate::width() const
 {
  return 0;
 }

 int FloatImagePrivate::height() const
 {
  return 0;
 }

 FloatImage::FloatImage()
 {

 }

 FloatImage::~FloatImage()
 {

 }

 int FloatImage::width() const
 {
  return 0;
 }

 int FloatImage::height() const
 {
  return 0;
 }

 void FloatImage::fill(const FloatColor& color)
 {

 }

 void FloatImage::resize()
 {

 }

 void FloatImage::resize(int width, int height)
 {

 }

};