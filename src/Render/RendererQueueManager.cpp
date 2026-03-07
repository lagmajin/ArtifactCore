module;
#define QT_NO_KEYWORDS
#include <QObject>
#include <QCoreApplication>
//#include <folly/th>
#include <tbb/tbb.h>
//#include <absl/container/>
module Render.Queue.Manager;

import Render.Settings;
import Render.JobModel;
import Utils.Id;
import Core.ThreadPool;
import std;
import Log;



namespace ArtifactCore {

  class RendererQueueManager::Impl{
  public:

    std::unique_ptr<RenderJobModel> jobModel;
    std::atomic_bool isRendering{ false };
    tbb::task_group renderTasks;
    RenderFrameFunc renderFrameFunc;

    Impl() : jobModel(std::make_unique<RenderJobModel>()) {}
    ~Impl() { isRendering = false; renderTasks.wait(); }

    void processQueue();
    RenderSettings makeDefaultSettings() const;
    void startRenderingAllQueue();
    RenderJobModel* model() const { return jobModel.get(); }
#ifdef _DEBUG
   void testRendering();
#else
   void testRendering() {};
#endif
  };

  void RendererQueueManager::setRenderFrameFunc(RenderFrameFunc func) {
    impl_->renderFrameFunc = func;
  }

  RenderSettings RendererQueueManager::Impl::makeDefaultSettings() const
  {
   RenderSettings settings;

   return settings;
  }

  void RendererQueueManager::Impl::processQueue()
  {
    auto& manager = RendererQueueManager::instance();
    auto& pool = ThreadPool::globalInstance();
    int jobCount = jobModel->rowCount();
    
    for (int i = 0; i < jobCount; ++i) {
        if (!isRendering) break;

        auto* job = jobModel->jobAt(i);
        if (job->status != RenderJobStatus::Queued) continue;

        jobModel->setJobStatus(i, RenderJobStatus::Rendering);
        
        // ジョブの設定を取得（本来はRenderSettingsから取得）
        int totalFrames = 300; // Mock: 10s at 30fps
        std::atomic<int> completedFrames{0};

        // 各フレームを並列にエンキュー
        for (int frame = 0; frame < totalFrames; ++frame) {
            if (!isRendering) break;

            pool.enqueueTask([this, i, frame, totalFrames, &completedFrames, job]() {
                if (!isRendering) return;
                
                // --- ACTUAL RENDERING CALL ---
                if (renderFrameFunc) {
                    renderFrameFunc(job->compositionId, frame, job->outputPath);
                }
                
                int current = ++completedFrames;
                jobModel->setJobProgress(i, (float)current / (float)totalFrames);
                
                if (current == totalFrames) {
                    jobModel->setJobStatus(i, RenderJobStatus::Done);
                }
            });
        }
        
        // 全フレームの完了を待機（または次のジョブへ）
        // pool.waitAll(); // ジョブごとに待機する場合
    }
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

   void RendererQueueManager::addJob(const ArtifactCore::Id& compositionId, const QString& name)
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