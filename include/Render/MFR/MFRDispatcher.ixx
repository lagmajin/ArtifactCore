module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

export module Core.Render.MFR.Dispatcher;

import Core.Render.MFR.Job;

namespace ArtifactCore::Render::MFR {

export class MFRDispatcher {
  struct State;
public:
  using ProgressCallback = std::function<void(const MFRProgress&)>;

  MFRDispatcher() : state_(std::make_shared<State>()) {}
  ~MFRDispatcher() { cancel(); }

  MFRJobResult executeBlocking(const MFRJobConfig& config,
                               FrameTask frameTask,
                               ProgressCallback progressCallback = {}) {
    MFRJobResult result;
    if (!frameTask || config.frameStep <= 0 || config.endFrame < config.startFrame) {
      return result;
    }
    const long long span = static_cast<long long>(config.endFrame) -
                           static_cast<long long>(config.startFrame);
    const long long total64 = span / config.frameStep + 1;
    if (total64 <= 0 || total64 > std::numeric_limits<int>::max()) return result;
    const long long lastFrame64 = static_cast<long long>(config.startFrame) +
        (total64 - 1) * static_cast<long long>(config.frameStep);
    if (lastFrame64 < std::numeric_limits<int>::min() ||
        lastFrame64 > std::numeric_limits<int>::max() ||
        lastFrame64 > static_cast<long long>(config.endFrame)) {
      return result;
    }
    const int total = static_cast<int>(total64);
    const int hardware = std::max(1, static_cast<int>(
        std::thread::hardware_concurrency()));
    int concurrency = config.maxConcurrentFrames > 0
        ? config.maxConcurrentFrames : std::max(1, hardware - 2);
    if (config.maxMemoryMB > 0 && config.estimatedMemoryPerFrame > 0) {
      const auto limited = config.maxMemoryMB / config.estimatedMemoryPerFrame;
      const int memoryLimited = limited >= static_cast<std::size_t>(
          std::numeric_limits<int>::max())
          ? std::numeric_limits<int>::max()
          : std::max(1, static_cast<int>(limited));
      concurrency = std::min(concurrency, memoryLimited);
    }
    if (config.framesAreDependent) concurrency = 1;
    concurrency = std::clamp(concurrency, 1, total);

    MFRProgress progress;
    progress.totalFrames = total;
    result.frames.resize(static_cast<std::size_t>(total));
    for (int index = 0; index < total; ++index) {
      result.frames[static_cast<std::size_t>(index)].frameNumber =
          static_cast<int>(static_cast<long long>(config.startFrame) +
                           static_cast<long long>(index) * config.frameStep);
    }
    auto state = state_;
    state->nextFrame.store(0);
    state->cancelled.store(false);
    const auto started = std::chrono::steady_clock::now();
    std::mutex resultMutex;
    std::mutex callbackMutex;

    auto worker = [&]() {
      while (!state->cancelled.load()) {
        const int index = state->nextFrame.fetch_add(1);
        if (index >= total) break;
        const int frame = static_cast<int>(static_cast<long long>(config.startFrame) +
                                           static_cast<long long>(index) * config.frameStep);
        MFRFrameResult frameResult;
        frameResult.frameNumber = frame;
        const auto frameStarted = std::chrono::steady_clock::now();
        const int attempts = std::clamp(config.maxRetryCount, 0, 100) + 1;
        for (int attempt = 0; attempt < attempts; ++attempt) {
          if (state->cancelled.load()) break;
          try {
            if (frameTask(frame)) {
              frameResult.success = true;
              break;
            }
            frameResult.errorMessage = QStringLiteral("Frame render failed");
          } catch (const std::exception& error) {
            frameResult.errorMessage = QStringLiteral("Frame render exception: ") +
                QString::fromUtf8(error.what());
          } catch (...) {
            frameResult.errorMessage =
                QStringLiteral("Frame render failed with an unknown exception");
          }
          if (!frameResult.success && attempt + 1 < attempts &&
              config.retryBackoffMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                std::min(config.retryBackoffMs, 60000)));
          }
        }
        frameResult.elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - frameStarted).count();
        {
          std::lock_guard<std::mutex> lock(resultMutex);
          result.frames[static_cast<std::size_t>(index)] = frameResult;
        }
        if (frameResult.success) progress.completedFrames.fetch_add(1);
        else {
          progress.failedFrames.fetch_add(1);
          if (!config.continueOnError) state->cancelled.store(true);
        }
        if (progressCallback) {
          std::lock_guard<std::mutex> callbackLock(callbackMutex);
          try {
            progressCallback(progress);
          } catch (...) {
            // Progress reporting must never terminate a render worker.
          }
        }
      }
    };
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(concurrency));
    for (int i = 0; i < concurrency; ++i) workers.emplace_back(worker);
    for (auto& thread : workers) thread.join();
    result.completedCount = progress.completedFrames.load();
    result.failedCount = progress.failedFrames.load();
    result.cancelled = state->cancelled.load() && result.failedCount == 0;
    result.totalElapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    double sequentialElapsedMs = 0.0;
    for (const auto& frame : result.frames) {
      sequentialElapsedMs += frame.elapsedMs;
    }
    result.speedupVsSequential = result.totalElapsedMs > 0.0
        ? static_cast<float>(sequentialElapsedMs / result.totalElapsedMs)
        : 1.0f;
    return result;
  }

  std::future<MFRJobResult> executeAsync(const MFRJobConfig& config,
                                         FrameTask frameTask,
                                         ProgressCallback progressCallback = {}) {
    const auto state = state_;
    return std::async(std::launch::async,
                      [state, config, frameTask = std::move(frameTask),
                       progressCallback = std::move(progressCallback)]() mutable {
                        MFRDispatcher worker;
                        worker.state_ = state;
                        return worker.executeBlocking(
                            config, std::move(frameTask),
                            std::move(progressCallback));
                      });
  }

  void cancel() { state_->cancelled.store(true); }
  bool isCancelled() const { return state_->cancelled.load(); }

private:
  struct State {
    std::atomic<int> nextFrame{0};
    std::atomic<bool> cancelled{false};
  };
  std::shared_ptr<State> state_;
};

} // namespace ArtifactCore::Render::MFR
