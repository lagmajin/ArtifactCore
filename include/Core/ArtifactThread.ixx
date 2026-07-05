module;
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <condition_variable>

export module Core.ArtifactThread;

import Core.ArtifactOptional;

export namespace ArtifactCore {

/// Recursive-capable mutex. Use with Lock for RAII.
class Mutex {
public:
    Mutex() : owner_(0), count_(0) {}
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void lock() {
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        if (owner_.load(std::memory_order_acquire) == tid) {
            ++count_;
            return;
        }
        while (true) {
            uint64_t expected = 0;
            if (owner_.compare_exchange_weak(expected, tid, std::memory_order_acq_rel)) {
                count_ = 1;
                return;
            }
            expected = 0;
            std::this_thread::yield();
        }
    }

    bool tryLock() {
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        uint64_t expected = 0;
        if (owner_.compare_exchange_strong(expected, tid, std::memory_order_acq_rel)) {
            count_ = 1;
            return true;
        }
        return false;
    }

    void unlock() {
        assert(owner_.load(std::memory_order_relaxed) == std::hash<std::thread::id>{}(std::this_thread::get_id())
               && "Mutex::unlock from non-owner thread");
        --count_;
        if (count_ == 0) owner_.store(0, std::memory_order_release);
    }

private:
    friend class Cond;
    std::atomic<uint64_t> owner_;
    int count_;
};

/// RAII lock guard. Auto-unlocks on destruction.
class Lock {
public:
    explicit Lock(Mutex& m) : mutex_(m) { mutex_.lock(); }
    ~Lock() { mutex_.unlock(); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
private:
    Mutex& mutex_;
};

/// Condition variable for thread synchronization.
class Cond {
public:
    Cond() = default;
    Cond(const Cond&) = delete;
    Cond& operator=(const Cond&) = delete;

    void wait(Lock& lock) {
        cv_.wait(cv_lock_, [&] { return notified_; });
        notified_ = false;
    }

    template <typename Predicate>
    void wait(Lock& lock, Predicate pred) {
        while (!pred()) { wait(lock); }
    }

    void wakeOne() {
        notified_ = true;
        cv_.notify_one();
    }

    void wakeAll() {
        notified_ = true;
        cv_.notify_all();
    }

private:
    std::condition_variable_any cv_;
    std::mutex cv_lock_;
    bool notified_ = false;
};

/// Simple worker thread wrapper.
class Thread {
public:
    Thread() = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    template <typename F>
    void start(F&& fn) {
        if (running_) return;
        running_ = true;
        thread_ = std::thread([this, fn = std::move(std::forward<F>(fn))]() {
            fn();
            running_ = false;
        });
    }

    void join() { if (thread_.joinable()) thread_.join(); running_ = false; }
    void detach() { if (thread_.joinable()) thread_.detach(); running_ = false; }
    bool isRunning() const { return running_.load(); }

private:
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace ArtifactCore
