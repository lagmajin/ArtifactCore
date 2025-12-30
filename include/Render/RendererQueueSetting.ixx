module;
#include <wobjectdefs.h>

#include <QString>
export module RendererQueueSetting;

import Utils;

import Utils.String.UniString;

export namespace ArtifactCore
{

 enum class eRendererState {

 };

 class RendererQueueSetting
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  RendererQueueSetting();
  RendererQueueSetting(const RendererQueueSetting& settings);
  ~RendererQueueSetting();
  QString queueName() const;
  template<StringLike T>
  void setRendererQueueName(const T& name);
  UniString outputDirectory() const;
  void setOutputDirectory(const UniString& dir);

 };




}