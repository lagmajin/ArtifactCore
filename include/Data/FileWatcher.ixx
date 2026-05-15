module;

#include <string>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <cstdint>

export module Data.FileWatcher;

export namespace ArtifactCore {

using FileChangeCallback = std::function<void(const std::string& path)>;

struct WatchedFile {
    std::filesystem::path path;
    std::filesystem::file_time_type lastWrite;
    FileChangeCallback callback;
};

class FileWatcher {
public:
    static FileWatcher& instance() {
        static FileWatcher inst;
        return inst;
    }

    void watch(const std::string& path, FileChangeCallback callback, int pollIntervalMs = 2000) {
        std::filesystem::path p(path);
        if (!std::filesystem::exists(p)) return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = watched_.find(path);
        if (it != watched_.end()) {
            it->second.callback = std::move(callback);
            return;
        }

        WatchedFile wf;
        wf.path = p;
        wf.lastWrite = std::filesystem::last_write_time(p);
        wf.callback = std::move(callback);
        watched_[path] = std::move(wf);

        if (!running_.load()) {
            running_.store(true);
            pollThread_ = std::thread([this, pollIntervalMs]() {
                pollLoop(pollIntervalMs);
            });
            pollThread_.detach();
        }
    }

    void unwatch(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        watched_.erase(path);
    }

    void unwatchAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        watched_.clear();
        running_.store(false);
    }

    bool isWatching(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return watched_.count(path) > 0;
    }

private:
    FileWatcher() = default;

    void pollLoop(int intervalMs) {
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

            std::vector<std::pair<std::string, FileChangeCallback>> triggered;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [path, wf] : watched_) {
                    if (!std::filesystem::exists(wf.path)) continue;

                    auto currentWrite = std::filesystem::last_write_time(wf.path);
                    if (currentWrite != wf.lastWrite) {
                        wf.lastWrite = currentWrite;
                        triggered.emplace_back(path, wf.callback);
                    }
                }
            }

            for (auto& [path, cb] : triggered) {
                if (cb) cb(path);
            }
        }
    }

    std::unordered_map<std::string, WatchedFile> watched_;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
    mutable std::mutex mutex_;
};

} // namespace ArtifactCore
