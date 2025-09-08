module;

#include <QString>
module Core.KeyFrame;


import std;



namespace Artifact {

class KeyFrame::Impl
{
private:

public:
 float time_ = 0.0f;
 std::any value;
};

 KeyFrame::KeyFrame()
 {

 }

 KeyFrame::~KeyFrame()
 {

 }

 void KeyFrame::setValue(const std::any& v)
 {

 }

 void KeyFrame::setTime(float t)
 {

 }

};