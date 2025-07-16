module;

#include <QObject>
#include "../Define/DllExportMacro.hpp"
export module Render.Queue.Manager;

import std;


export namespace ArtifactCore {

 class LIBRARY_DLL_API RendererQueueManager:public QObject {
 private:
  class Impl;
  Impl* impl_;
  RendererQueueManager(QObject* parent = nullptr);
  //~RendererQueueManager();
  RendererQueueManager(const RendererQueueManager&) = delete;
  RendererQueueManager& operator=(const RendererQueueManager&) = delete;

 public:
  //explicit RendererQueueManager(QObject* parent=nullptr);
  ~RendererQueueManager();
  //RenderJobModel* model();
  static RendererQueueManager& instance(); // シングルトンインスタンス取得

 };

 

}

