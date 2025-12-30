module;

module RendererQueueSetting;




namespace ArtifactCore
{


 class RendererQueueSetting::Impl
 {
 private:
  QString name_;
  UniString outputDirectory_;

 public:
  Impl();
  ~Impl();
  QString rendererQueueName() const;
  void setRendererQueueName(const QString& name);
  UniString outputDirectory() const;
  void setOutputDirectory(const UniString& dir);


 };

 RendererQueueSetting::Impl::Impl()
 {

 }

 RendererQueueSetting::Impl::~Impl()
 {

 }

 void RendererQueueSetting::Impl::setRendererQueueName(const QString& name)
 {

 }

 QString RendererQueueSetting::Impl::rendererQueueName() const
 {
  return name_;
 }

 UniString RendererQueueSetting::Impl::outputDirectory() const
 {

  return outputDirectory_;
 }

 void RendererQueueSetting::Impl::setOutputDirectory(const UniString& dir)
 {
  outputDirectory_ = dir;
 }

 RendererQueueSetting::RendererQueueSetting():impl_(new Impl())
 {

 }

 RendererQueueSetting::RendererQueueSetting(const RendererQueueSetting& settings) :impl_(new Impl())
 {

 }

 RendererQueueSetting::~RendererQueueSetting()
 {
  delete impl_;
 }

 QString RendererQueueSetting::queueName() const
 {
  return impl_->rendererQueueName();
 }

 template<StringLike T>
 void RendererQueueSetting::setRendererQueueName(const T& name)
 {
  impl_->setRendererQueueName(name);

 }

 UniString RendererQueueSetting::outputDirectory() const
 {

  return impl_->outputDirectory();
 }

 void RendererQueueSetting::setOutputDirectory(const UniString& dir)
 {
  impl_->setOutputDirectory(dir);
 }




};