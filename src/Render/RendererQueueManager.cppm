module;
#define QT_NO_KEYWORDS
//#include <folly/th>
//#include <absl/container/>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
   
// Qt headers must stay in the global fragment for module ABI stability.
#include <QObject>
#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
module Render.Queue.Manager;

import Render.Settings;
import Render.JobModel;
import Memory.TrackedPtr;
import Utils.Id;
import Core.ThreadPool;
import Core.Render.MFR.Dispatcher;
import Core.Render.MFR.Job;



namespace ArtifactCore {

  class RendererQueueManager::Impl{
  public:

    std::unique_ptr<RenderJobModel> jobModel;
    std::atomic_bool isRendering{ false };
    std::thread renderThread_;
    RenderFrameFunc renderFrameFunc;

    Impl() : jobModel(std::make_unique<RenderJobModel>()) {}
    ~Impl() {
      isRendering = false;
      if (renderThread_.joinable()) {
        renderThread_.join();
      }
    }

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
    struct RenderingStateGuard {
      std::atomic_bool& state;
      ~RenderingStateGuard() { state = false; }
    } stateGuard{isRendering};
    auto& manager = RendererQueueManager::instance();
    int jobCount = jobModel->rowCount();
    
    for (int i = 0; i < jobCount; ++i) {
        if (!isRendering) break;

        auto* job = jobModel->jobAt(i);
        if (job->status != RenderJobStatus::Queued) continue;

        if (!renderFrameFunc) {
            jobModel->setJobStatus(i, RenderJobStatus::Error);
            continue;
        }

        jobModel->setJobStatus(i, RenderJobStatus::Rendering);
        
        ArtifactCore::Render::MFR::MFRJobConfig config;
        config.startFrame = job->startFrame;
        config.endFrame = job->endFrame;
        config.frameStep = job->frameStep;
        config.maxConcurrentFrames = job->multiFrameEnabled
            ? job->mfrConcurrentFrames : 1;
        config.maxMemoryMB = job->mfrMemoryLimitMB;
        config.retryBackoffMs = job->mfrRetryBackoffMs;
        config.maxRetryCount = 0;
        config.continueOnError = false;
        ArtifactCore::Render::MFR::MFRDispatcher dispatcher;
        const auto mfrResult = dispatcher.executeBlocking(
            config,
            [this, job](int frame) {
                if (!isRendering || !renderFrameFunc) return false;
                renderFrameFunc(job->compositionId, frame, job->outputPath);
                return true;
            },
            [this, i](const ArtifactCore::Render::MFR::MFRProgress& progress) {
                auto* model = jobModel.get();
                const float normalizedProgress = progress.percentComplete() / 100.0f;
                QMetaObject::invokeMethod(
                    model,
                    [model, i, normalizedProgress]() {
                        model->setJobProgress(i, normalizedProgress);
                    },
                    Qt::QueuedConnection);
            });
        if (!isRendering) {
            jobModel->setJobStatus(i, RenderJobStatus::Canceled);
        } else if (mfrResult.failedCount > 0) {
            jobModel->setJobStatus(i, RenderJobStatus::Error);
        } else if (isRendering && mfrResult.completedCount ==
                   static_cast<int>(mfrResult.frames.size())) {
            jobModel->setJobStatus(i, RenderJobStatus::Done);
        }
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
   delete impl_;
  }
 
  RendererQueueManager& RendererQueueManager::instance()
  {
   static RendererQueueManager s_instance(nullptr);
   return s_instance;
  }

   void RendererQueueManager::startRenderingAllQueue()
   {
    if (impl_->isRendering) return;
    if (impl_->renderThread_.joinable()) {
        impl_->renderThread_.join();
    }
    impl_->isRendering = true;
    
    // Run in a background thread.
    impl_->renderThread_ = std::thread([this]() {
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

   void RendererQueueManager::addJob(const ArtifactCore::Id& compositionId,
                                     const QString& name, int startFrame,
                                     int endFrame, int frameStep)
   {
    impl_->jobModel->addJob(compositionId, name, startFrame, endFrame,
                            frameStep);
   }

   bool RendererQueueManager::setJobMfrSettings(
       int row, bool enabled, int maxConcurrentFrames,
       std::size_t memoryLimitMB, int retryBackoffMs)
   {
    return impl_->jobModel->setJobMfrSettings(
        row, enabled, maxConcurrentFrames, memoryLimitMB, retryBackoffMs);
   }


  RenderJobModel* RendererQueueManager::model()
  {
   return impl_->model();
  }

#ifdef _DEBUG
  void RendererQueueManager::testRendering()
  {
  }
#endif




};
