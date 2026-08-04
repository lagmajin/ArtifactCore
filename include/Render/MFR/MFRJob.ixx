module;
#include <atomic>
#include <cstddef>
#include <functional>
#include <QString>
#include <vector>

export module Core.Render.MFR.Job;

namespace ArtifactCore::Render::MFR {

export using FrameTask = std::function<bool(int)>;

export struct MFRJobConfig {
  int startFrame = 0;
  int endFrame = 100;
  int maxConcurrentFrames = 0;
  int frameStep = 1;
  std::size_t maxMemoryMB = 8192;
  std::size_t estimatedMemoryPerFrame = 256;
  bool framesAreDependent = false;
  int maxRetryCount = 2;
  int retryBackoffMs = 0;
  bool continueOnError = false;
};

export struct MFRProgress {
  std::atomic<int> completedFrames{0};
  std::atomic<int> failedFrames{0};
  int totalFrames = 0;

  float percentComplete() const {
    const int finished = completedFrames.load() + failedFrames.load();
    return totalFrames > 0 ? 100.0f * static_cast<float>(finished) /
                               static_cast<float>(totalFrames)
                           : 0.0f;
  }
};

export struct MFRFrameResult {
  int frameNumber = 0;
  bool success = false;
  QString errorMessage;
  double elapsedMs = 0.0;
  std::size_t peakMemoryBytes = 0;
};

export struct MFRJobResult {
  std::vector<MFRFrameResult> frames;
  double totalElapsedMs = 0.0;
  std::size_t totalPeakMemoryBytes = 0;
  int completedCount = 0;
  int failedCount = 0;
  float speedupVsSequential = 1.0f;
  bool cancelled = false;
};

} // namespace ArtifactCore::Render::MFR
