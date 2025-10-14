module;

module Frame.FrameRate;



namespace ArtifactCore {

 class Framerate::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };



 Framerate::Framerate()
 {

 }

 Framerate::Framerate(float frameRate)
 {

 }

 Framerate::Framerate(const QString& str)
 {

 }


 Framerate::Framerate(const Framerate& frameRate)
 {

 }

 Framerate::~Framerate()
 {

 }

 void Framerate::setFramerate(float frame /*= 30.0f*/)
 {

 }

 void Framerate::speedUp(float frame /*= 1.0f*/)
 {

 }

 void Framerate::speedDown(float frame /*= 1.0f*/)
 {

 }

 void Framerate::setFromJson(const QJsonObject& object)
 {

 }

 void Framerate::readFromJson(QJsonObject& object) const
 {

 }

 void Framerate::writeToJson(QJsonObject& object) const
 {

 }

 void Framerate::setFromString(const QString& framerate)
 {

 }

}