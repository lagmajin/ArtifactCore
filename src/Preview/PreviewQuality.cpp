module;

module Preview.Quality;


namespace ArtifactCore {

 class PreviewQuality::Impl {
 private:

 public:
  Impl();
  ~Impl();
  Scale scale = Scale::Full;
 };

 PreviewQuality::Impl::Impl()
 {

 }

 PreviewQuality::Impl::~Impl()
 {

 }
	
 PreviewQuality::PreviewQuality():impl_(new Impl)
 {

 }

 PreviewQuality::PreviewQuality(const PreviewQuality& other) :impl_(new Impl)
 {

 }

 PreviewQuality::PreviewQuality(Scale scale) :impl_(new Impl)
 {

 }

 PreviewQuality::~PreviewQuality()
 {
  delete impl_;
 }

 PreviewQuality::Scale PreviewQuality::quality() const
 {
  return impl_->scale;
 }

 void PreviewQuality::setQuality(Scale scale)
 {

 }

 bool PreviewQuality::operator==(const PreviewQuality& o) const
 {
  return impl_->scale == o.impl_->scale;
 }

 bool PreviewQuality::operator!=(const PreviewQuality& o) const
 {
  return !((*this) == o);
 }

};