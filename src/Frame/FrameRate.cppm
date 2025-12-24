module;

module Frame.Rate;



namespace ArtifactCore {

 class FrameRate::Impl {
 private:
  
 public:
  Impl();
  ~Impl();
  float frameRate_ = 0.0f;
 };

 FrameRate::Impl::Impl()
 {

 }

 FrameRate::Impl::~Impl()
 {

 }

 FrameRate::FrameRate():impl_(new Impl)
 {

 }

 FrameRate::FrameRate(float frameRate) :impl_(new Impl)
 {

 }

 FrameRate::FrameRate(const QString& str) :impl_(new Impl)
 {

 }


 FrameRate::FrameRate(const FrameRate& frameRate) :impl_(new Impl)
 {

 }

 FrameRate::FrameRate(FrameRate&& framerate) noexcept :impl_(new Impl)
 {

 }

 FrameRate::~FrameRate()
 {
  delete impl_;
 }

 void FrameRate::setFrameRate(float frame /*= 30.0f*/)
 {
     impl_->frameRate_ = frame;
 }

 void FrameRate::speedUp(float frame /*= 1.0f*/)
 {

 }

 void FrameRate::speedDown(float frame /*= 1.0f*/)
 {

 }

 void FrameRate::setFromJson(const QJsonObject& object)
 {

 }

 void FrameRate::readFromJson(QJsonObject& object) const
 {

 }

 void FrameRate::writeToJson(QJsonObject& object) const
 {

 }

 void FrameRate::setFromString(const QString& framerate)
 {

 }

 FrameRate& FrameRate::operator=(float rate)
 {
  return *this;
 }

 FrameRate& FrameRate::operator=(const QString& str)
 {
  return *this;
 }

 FrameRate& FrameRate::operator=(const FrameRate& framerate)
 {
  return *this;
 }

 FrameRate& FrameRate::operator=(FrameRate&& framerate) noexcept
 {
  return *this;
 }

 UniString FrameRate::toString() const
 {
  return UniString();
 }

 float FrameRate::framerate() const
 {
  return impl_->frameRate_;
 }

UniString FrameRate::toDisplayString(bool includeDropframe /*= true*/) const
 {

 return UniString();
 }

 bool operator==(const FrameRate& framerate1, const FrameRate& framerate2)
 {
  return framerate1.framerate() == framerate2.framerate();
 }

 bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2)
 {
  return !(framerate1 == framerate2);
 }

}