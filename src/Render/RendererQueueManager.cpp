module;
#include <QObject>
#include <QCoreApplication>
//#include <folly/th>

//#include <absl/container/>
module Render.Queue.Manager;

import Render.Settings;
import Render.JobModel;
import Render.Queue.Manager;


namespace ArtifactCore {

  class RendererQueueManager::Impl{
  private:

  public:
   RenderSettings makeDefaultSettings() const;
   void startRenderingAllQueue();

   RenderJobModel* model() const;
#ifdef _DEBUG
   void testREndering();
#elif
   void testRendering() {};
#endif
  };

  RenderSettings RendererQueueManager::Impl::makeDefaultSettings() const
  {
   RenderSettings settings;

   return settings;
  }

  void RendererQueueManager::Impl::startRenderingAllQueue()
  {

  }

  RenderJobModel* RendererQueueManager::Impl::model() const
  {

   return nullptr;
  }

  RendererQueueManager::RendererQueueManager(QObject* parent/*=nullptr*/):QObject(parent)
  {

  }


  RendererQueueManager::~RendererQueueManager()
  {

  }
 
  RendererQueueManager& RendererQueueManager::instance()
  {
   static RendererQueueManager s_instance(nullptr);
   return s_instance;
  }

  void RendererQueueManager::startRendering()
  {

  }

  void RendererQueueManager::clearRenderQueue()
  {

  }

  void RendererQueueManager::startRenderingAllQueue()
  {

  }


#ifdef _DEBUG
  void RendererQueueManager::testRendering()
  {

  }

  RenderJobModel* RendererQueueManager::model()
  {

   return impl_->model();
  }

#endif




};