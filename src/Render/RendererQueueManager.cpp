module;
#include <QObject>
#include <QCoreApplication>
//#include <folly/th>

//#include <absl/container/>
module Render.Queue.Manager;

import Render.Settings;
import Render.JobModel;
import Render.Queue.Manager;
import Utils.Id;
import std;
import Log;
#include <tbb/tbb.h>


namespace ArtifactCore {

  class RendererQueueManager::Impl{
  private:

    std::unique_ptr<RenderJobModel> jobModel;
    std::atomic_bool isRendering{ false };
    tbb::task_group renderTasks;

    Impl() : jobModel(std::make_unique<RenderJobModel>()) {}
    ~Impl() { isRendering = false; renderTasks.wait(); }

    void processQueue();
    RenderSettings makeDefaultSettings() const;
    void startRenderingAllQueue();
    RenderJobModel* model() const { return jobModel.get(); }
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

  void RendererQueueManager::Impl::processQueue()
  {
    auto& manager = RendererQueueManager::instance();
    int jobCount = jobModel->rowCount();
    
    for (int i = 0; i < jobCount; ++i) {
        if (!isRendering) break;

        auto* job = jobModel->jobAt(i);
        if (job->status != RenderJobStatus::Queued) continue;

        jobModel->setJobStatus(i, RenderJobStatus::Rendering);
        
        // --- REAL RENDERING LOOP ---
        // In AE, this renders individual frames of the composition
        int totalFrames = 300; // Mock: 10s at 30fps
        for (int frame = 0; frame < totalFrames; ++frame) {
            if (!isRendering) break;
            
            // 1. Set Composition Time
            // 2. Perform Offscreen Render
            // 3. Save to disk (e.g., job->outputPath)
            
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps 
            
            jobModel->setJobProgress(i, (float)frame / (float)totalFrames);
        }
        
        if (isRendering) {
            jobModel->setJobStatus(i, RenderJobStatus::Done);
            jobModel->setJobProgress(i, 1.0f);
        } else {
            jobModel->setJobStatus(i, RenderJobStatus::Canceled);
        }
    }
    isRendering = false;
  }

  // This method is now inlined in the class definition.
  // RenderJobModel* RendererQueueManager::Impl::model() const
  // {
  //
  //  return nullptr;
  // }

  RendererQueueManager::RendererQueueManager(QObject* parent/*=nullptr*/):QObject(parent), impl_(new Impl())
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

   void RendererQueueManager::startRenderingAllQueue()
   {
    if (impl_->isRendering) return;
    impl_->isRendering = true;
    
    // Use TBB task_group to run in background
    impl_->renderTasks.run([this]() { 
        impl_->processQueue(); 
    });
   }

   void RendererQueueManager::startRendering()
   {
    startRenderingAllQueue();
   }

   bool RendererQueueManager::isRenderNow() const 
   {
    return impl_->isRendering;
   }

   void RendererQueueManager::clearRenderQueue()
   {
    impl_->jobModel->clearJobs();
   }

   void RendererQueueManager::addJob(const Id& compositionId, const QString& name)
   {
    impl_->jobModel->addJob(compositionId, name);
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