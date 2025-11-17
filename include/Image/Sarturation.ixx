module;
#include <QString>

export module Color.Saturation;

import std;

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

