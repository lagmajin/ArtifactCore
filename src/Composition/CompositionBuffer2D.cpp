

#include "../../include/Composition/CompositionBuffer2D.hpp"




namespace ArtifactCore {

 class CompositionBuffer2DPrivate {
 private:
  Halide::Buffer<float> b_;
 public:
  CompositionBuffer2DPrivate(int width,int height);
  ~CompositionBuffer2DPrivate();
  void clear();
  //void setClearColor(float = 0.0f);
 };

 CompositionBuffer2DPrivate::CompositionBuffer2DPrivate(int width, int height)
 {

 }

 CompositionBuffer2DPrivate::~CompositionBuffer2DPrivate()
 {

 }

 void CompositionBuffer2DPrivate::clear()
 {

 }

 CompositionBuffer2D::CompositionBuffer2D(int width, int height):pImpl_(new CompositionBuffer2DPrivate(width,height))
 {

 }

 CompositionBuffer2D::~CompositionBuffer2D()
 {

 }

 void CompositionBuffer2D::setEngine(eEngineBackend backend/*=Halide*/)
 {

 }

 void CompositionBuffer2D::clear()
 {

 }



 void CompositionBuffer2D::setClearColor(const FloatRGBA rgba)
 {

 }

}