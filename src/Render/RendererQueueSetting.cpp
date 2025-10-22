module;

module RendererQueueSetting;




namespace ArtifactCore
{


 class RendererQueueSetting::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 RendererQueueSetting::Impl::Impl()
 {

 }

 RendererQueueSetting::Impl::~Impl()
 {

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

};