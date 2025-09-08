module;
export module Event.Bus;

export namespace ArtifactCore
{

 class EventBus
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  EventBus();
  ~EventBus();
 };








};