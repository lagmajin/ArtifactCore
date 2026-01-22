module;
#define QT_NO_KEYWORDS
#include <QString>

export module Color.Saturation;

import std;
import Utils.String.UniString;


export namespace ArtifactCore {

	
	
 class Saturation
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  Saturation();
  explicit Saturation(float s);
  ~Saturation();
  float saturation() const;
  void setSaturation(float s); // 0..1 にクランプ

  //operator ==
  bool operator==(const Saturation& other) const;
  
  bool operator!=(const Saturation& other) const;

  bool operator>(const Saturation& other) const
  {
   return saturation() > other.saturation();
  }

  //operator >=
  bool operator>=(const Saturation& other) const
  {
   return saturation() >= other.saturation();
  }

   
  bool operator<(const Saturation& other) const
  {
   return saturation() < other.saturation();
  }

 };

};

