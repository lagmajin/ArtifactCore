//#include "../../include/Color/FloatRGBA.hpp"



module FloatRGBA;

namespace ArtifactCore {

 class FloatRGBAPrivate {
 public:
  float r_, g_, b_, a_ = 0.0f;

  void setRed(float r);
  void setGreen(float g);
  void setBlue(float b);
  void setApha(float a);
  void setRGBA(float r, float g, float b = 0.0f, float a = 0.0f);
 };

 void FloatRGBAPrivate::setRed(float r)
 {
  r_ = r;
 }

 void FloatRGBAPrivate::setGreen(float g)
 {
  g_ = g;
 }

 void FloatRGBAPrivate::setBlue(float b)
 {
  b_ = b;
 }

 void FloatRGBAPrivate::setApha(float a)
 {
  a_ = a;
 }

 void FloatRGBAPrivate::setRGBA(float r, float g, float b /*= 0.0f*/, float a /*= 0.0f*/)
 {

 } 
 
 FloatRGBA::FloatRGBA()
 {

 }

 FloatRGBA::FloatRGBA(const FloatRGBA& rgba)
 {

 }

 FloatRGBA::FloatRGBA(FloatRGBA&& rgba)
 {

 }

 FloatRGBA::~FloatRGBA()
 {



 }

 void FloatRGBA::setRed(float r)
 {

 }

 void FloatRGBA::setGreen(float g)
 {

 }

 void FloatRGBA::setBlue(float b)
 {

 }

 void FloatRGBA::setApha(float a)
 {

 }

 void FloatRGBA::setRGBA(float r, float g, float b /*= 0.0f*/, float a /*= 0.0f*/)
 {

 }

 void FloatRGBA::setFromFloatColor(const FloatColor& color)
 {

 }

 void FloatRGBA::setFromRandom()
 {

 }

};

