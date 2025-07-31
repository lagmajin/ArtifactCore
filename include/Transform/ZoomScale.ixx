module;



export module Core.Scale2D;


namespace ArtifactCore {

 class ZoomScale2D {
 private:
  class Impl;
  Impl* impl_;
 public:
  ZoomScale2D();
  ~ZoomScale2D();
  void ZoomIn(float factor);
  void ZoomOut(float factor);
  float GetScale() const;


 };








};