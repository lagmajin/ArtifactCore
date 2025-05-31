module;
#include <memory>
#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#endif
export module ImageF32x4;




export namespace ArtifactCore {

 class ImageF32x4 {
 private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
 public:
  ImageF32x4();
  ImageF32x4(int width, int height);
  ~ImageF32x4();
  int width() const;
  int height() const;

 };





}