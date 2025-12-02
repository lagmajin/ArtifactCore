module;

module Math.Rotation;

namespace ArtifactCore
{
 class Rotation::Impl
 {
 private:
 	
 public:
  Impl();
  ~Impl();
 };

 Rotation::Impl::Impl()
 {

 }

 Rotation::Impl::~Impl()
 {

 }

 Rotation::Rotation():impl_(new Impl())
 {

 }

 Rotation::Rotation(const Rotation& other) :impl_(new Impl())
 {

 }

 Rotation::Rotation(Rotation&& other) noexcept :impl_(new Impl())
 {

 }

 Rotation::~Rotation()
 {
  delete impl_;
 }

};
