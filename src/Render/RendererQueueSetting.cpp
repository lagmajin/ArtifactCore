module;

module RendererQueueSetting;




namespace ArtifactCore
{


 class RendererQueueSetting::Impl
 {
 private:
  QString name_;
 public:
  Impl();
  ~Impl();
  QString rendererQueueName() const;
  void setRendererQueueName(const QString& name);
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
};