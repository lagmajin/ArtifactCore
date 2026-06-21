module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

module Thread.PreciseTicker;

namespace ArtifactCore {

class PreciseTicker::Impl {
public:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  Duration interval_{Duration(16)};
  Callback callback_;
  std::thread worker_;
  std::atomic_bool running_{false};
  bool stopRequested_ = false;
  bool rescheduleRequested_ = false;

  void run() {
    auto nextTick = std::chrono::steady_clock::now() + interval_;

    for (;;) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait_until(lock, nextTick, [this]() {
        return stopRequested_ || rescheduleRequested_;
      });

      if (stopRequested_) {
        break;
      }

      if (rescheduleRequested_) {
        rescheduleRequested_ = false;
        nextTick = std::chrono::steady_clock::now() + interval_;
        continue;
      }

      Callback callback = callback_;
      const auto interval = interval_;
      lock.unlock();

      if (callback) {
        callback();
      }

      nextTick += interval;
      const auto now = std::chrono::steady_clock::now();
      if (nextTick < now) {
        nextTick = now + interval;
      }
    }
  }
};

PreciseTicker::PreciseTicker() : impl_(new Impl()) {}

PreciseTicker::~PreciseTicker() {
  stop();
  delete impl_;
}

void PreciseTicker::setInterval(Duration interval) {
  if (!impl_) {
    return;
  }
  if (interval <= Duration::zero()) {
    interval = Duration(1);
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->interval_ = interval;
    impl_->rescheduleRequested_ = true;
  }
  impl_->cv_.notify_all();
}

PreciseTicker::Duration PreciseTicker::interval() const {
  if (!impl_) {
    return Duration(16);
  }
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  return impl_->interval_;
}

void PreciseTicker::setCallback(Callback callback) {
  if (!impl_) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex_);
  impl_->callback_ = std::move(callback);
}

void PreciseTicker::start() {
  if (!impl_ || impl_->running_.exchange(true)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->stopRequested_ = false;
    impl_->rescheduleRequested_ = false;
  }

  impl_->worker_ = std::thread([impl = impl_]() { impl->run(); });
}

void PreciseTicker::stop() {
  if (!impl_ || !impl_->running_.exchange(false)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->stopRequested_ = true;
    impl_->rescheduleRequested_ = false;
  }
  impl_->cv_.notify_all();

  if (impl_->worker_.joinable()) {
    impl_->worker_.join();
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->stopRequested_ = false;
  }
}

bool PreciseTicker::isRunning() const {
  return impl_ && impl_->running_.load(std::memory_order_acquire);
}

}
