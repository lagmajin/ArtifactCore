module;

export module RendererQueueSetting;

export namespace ArtifactCore
{
 class RendererQueueSetting
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  RendererQueueSetting();
  RendererQueueSetting(const RendererQueueSetting& settings);
  ~RendererQueueSetting();
 };
	
}