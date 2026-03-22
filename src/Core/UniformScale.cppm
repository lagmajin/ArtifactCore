module;
module Core.UniformScale;

namespace ArtifactCore
{
 class UniformScale::Impl
 {
 private:
  float scale_ = 1.0f;
 public:
  Impl();
  ~Impl();
  float scale() const;
 };

 UniformScale::Impl::Impl()
 {

 }

 UniformScale::Impl::~Impl()
 {

 }

 UniformScale::UniformScale():impl_(new Impl())
 {

 }

 UniformScale::~UniformScale()
 {
  delete impl_;
 }

};
