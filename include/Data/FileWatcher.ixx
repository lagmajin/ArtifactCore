module;

#include <string>
#include <functional>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

export module Data.FileWatcher;

import Core.ArtifactString;
import Container.Debug;
import Container.NamedVector;
import Container.NameMap;

export namespace ArtifactCore {

using FileChangeCallback = std::function<void(const String& path)>;

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

    void watch(const String& path, FileChangeCallback callback, int pollIntervalMs = 2000) {
        const std::string pathStd = toStdString(path);
        std::filesystem::path p(pathStd);
        if (!std::filesystem::exists(p)) return;

        std::lock_guard<std::mutex> lock(mutex_);

        if (WatchedFile* const existing = watched_.findValue(pathStd)) {
            existing->callback = std::move(callback);
            return;
        }

        WatchedFile wf;
        wf.path = p;
        wf.lastWrite = std::filesystem::last_write_time(p);
        wf.callback = std::move(callback);
        watched_[pathStd] = std::move(wf);

        if (!running_.load()) {
            running_.store(true);
            const std::uint64_t generation = generation_.fetch_add(1) + 1;
            pollThread_ = std::thread([this, pollIntervalMs, generation]() {
                pollLoop(pollIntervalMs, generation);
            });
            pollThread_.detach();
        }
    }

    void unwatch(const String& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        watched_.erase(toStdString(path));
    }

    void unwatchAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        watched_.clear();
        generation_.fetch_add(1);
        running_.store(false);
    }

    bool isWatching(const String& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return watched_.contains(toStdString(path));
    }

private:
    FileWatcher() = default;

    void pollLoop(int intervalMs, std::uint64_t generation) {
        while (running_.load() && generation_.load() == generation) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            if (!running_.load() || generation_.load() != generation) break;

            NamedVector<std::pair<String, FileChangeCallback>> triggered;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [path, wf] : watched_) {
                    if (!std::filesystem::exists(wf.path)) continue;

                    auto currentWrite = std::filesystem::last_write_time(wf.path);
                    if (currentWrite != wf.lastWrite) {
                        wf.lastWrite = currentWrite;
                        triggered.emplace_back(String(path), wf.callback);
                    }
                }
            }

            for (auto& [path, cb] : triggered) {
                if (cb) cb(path);
            }
        }
    }

    NameMap<std::string, WatchedFile> watched_{
        ContainerName{"Data.FileWatcher.watched"}};
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> generation_{0};
    std::thread pollThread_;
    mutable std::mutex mutex_;
};

} // namespace ArtifactCore
