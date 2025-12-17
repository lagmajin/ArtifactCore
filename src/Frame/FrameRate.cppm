module;

module Frame.Rate;



namespace ArtifactCore {

 class FrameRate::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };



 FrameRate::FrameRate()
 {

 }

 FrameRate::FrameRate(float frameRate)
 {

 }

 FrameRate::FrameRate(const QString& str)
 {

 }


 FrameRate::FrameRate(const FrameRate& frameRate)
 {

 }

 FrameRate::FrameRate(FrameRate&& framerate) noexcept
 {

 }

 FrameRate::~FrameRate()
 {

 }

 void FrameRate::setFrameRate(float frame /*= 30.0f*/)
 {

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

 bool operator==(const FrameRate& framerate1, const FrameRate& framerate2)
 {
  return true;
 }

 bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2)
 {
  return !(framerate1 == framerate2);
 }

}