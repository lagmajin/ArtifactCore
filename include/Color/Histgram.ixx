module;
export module Float32x4Histogram;



namespace ArtifactCore {



 export class Float32x4Histogram {
 private:
  class Impl;
  Impl* d;

 public:
  explicit Float32x4Histogram(int bins = 1024);
  ~Float32x4Histogram();
  void reset();


 };

}