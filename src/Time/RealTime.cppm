module;

module Time.Real;



namespace ArtifactCore
{
 class RealTime::Impl {
 public:
  double currentTime = 0.0;
  double lastUpdateTime = 0.0;
  double deltaTime = 0.0;
  double timeScale = 1.0;
  bool paused = false;

  Impl() = default;
  ~Impl() = default;
 };

 // RealTime実装

 RealTime::RealTime()
  : impl_(new Impl())
 {
 }

 RealTime::~RealTime()
 {
  delete impl_;
 }

 void RealTime::update(double newTime)
 {
  if (impl_->paused) {
   impl_->deltaTime = 0.0;
  }
  else {
   impl_->deltaTime = (newTime - impl_->currentTime) * impl_->timeScale;
   impl_->currentTime = newTime;
  }
  impl_->lastUpdateTime = newTime;
 }

 void RealTime::pause()
 {
  impl_->paused = true;
 }

 void RealTime::resume()
 {
  impl_->paused = false;
 }

 void RealTime::setTimeScale(double scale)
 {
  impl_->timeScale = scale;
 }

 double RealTime::getCurrentTime() const
 {
  return impl_->currentTime;
 }

 double RealTime::getDeltaTime() const
 {
  return impl_->deltaTime;
 }

 bool RealTime::isPaused() const
 {
  return impl_->paused;
 }

};