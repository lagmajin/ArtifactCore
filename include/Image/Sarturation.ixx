module;

export module Color.Saturation;



export namespace ArtifactCore {

 class Saturation
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  Saturation();
  ~Saturation();
  float saturation() const;
  void setSaturation(float s); // 0..1 にクランプ
  
 };

};

