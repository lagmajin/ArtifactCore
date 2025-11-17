module ;

module Color.Saturation;

import std;

namespace ArtifactCore
{
 class Saturation::Impl
 {

 public:
  Impl();
  ~Impl();
  float value_{ 0.0f };
 };

 Saturation::Impl::Impl()
 {

 }

 Saturation::Impl::~Impl()
 {

 }

 Saturation::~Saturation()
 {
  delete impl_;
 }

 Saturation::Saturation() :impl_(new Impl())
 {
  
 }

 void Saturation::setSaturation(float s)
 {

 }

}