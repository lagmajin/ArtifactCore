module;
#include <HalideBuffer.h>
#include <cassert>
#include <opencv2/opencv.hpp>

export module HalideImageF32x4RGBA;



export namespace ArtifactCore {

// class HlideImageF32x4RGBA;

 class HlideImageF32xRGBA {
 
 public:

  HlideImageF32xRGBA(int width, int height);
  ~HlideImageF32xRGBA();
 
 private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
 
 };


}