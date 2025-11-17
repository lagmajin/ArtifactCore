module;

#include <QObject>
#include "../Define/DllExportMacro.hpp"
export module Render.Queue.Manager;

import std;

import Render.JobModel;
import RendererQueueSetting;

export namespace ArtifactCore {

 //using namespace ArtifactCore;

 class LIBRARY_DLL_API RendererQueueManager:public QObject {
 private:
  class Impl;
  Impl* impl_;
  explicit RendererQueueManager(QObject* parent = nullptr);
  //~RendererQueueManager();
  RendererQueueManager(const RendererQueueManager&) = delete;
  RendererQueueManager& operator=(const RendererQueueManager&) = delete;

 public:
  //explicit RendererQueueManager(QObject* parent=nullptr);
  ~RendererQueueManager();
  RenderJobModel* model();
  static RendererQueueManager& instance();
  
  void startRendering();
  void startRenderingAllQueue();
  void addRendering();
  void clearRenderQueue();
  bool isRenderNow() const;
#ifdef _DEBUG
  void testRendering();
#elif
  void testRendering() {};
#endif
 };


 };

 



