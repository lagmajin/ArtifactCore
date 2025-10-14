module;
#include <algorithm>
#include <QString>


module Core.Opacity;



namespace ArtifactCore
{
 class Opacity::Impl
 {
 private:

 public:
  float opacity_ = 0.0f;
 };


 Opacity::Opacity():impl_(new Impl())
 {

 }

 Opacity::Opacity(const Opacity& other) :impl_(new Impl())
 {

 }

 Opacity::~Opacity()
 {
delete impl_;
 }



};