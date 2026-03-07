module;
#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>

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
export module ArtifactCore.Utils.PerformanceProfiler;





namespace ArtifactCore {

/**
 * @brief Simple performance sample data
 */
export struct PerformanceSample {
    std::string name;
    double durationMs;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Registry to collect and provide performance metrics
 */
export class PerformanceRegistry {
public:
    static PerformanceRegistry& instance() {
        static PerformanceRegistry inst;
        return inst;
    }

    void recordSample(const std::string& name, double durationMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_[name] = { name, durationMs, std::chrono::system_clock::now() };
        
        // Keep a short history for trend analysis if needed
        history_[name].push_back(durationMs);
        if (history_[name].size() > 100) {
            history_[name].erase(history_[name].begin());
        }
    }

    std::map<std::string, PerformanceSample> getLatestSamples() {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_;
    }

    std::vector<double> getHistory(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_[name];
    }

private:
    std::mutex mutex_;
    std::map<std::string, PerformanceSample> samples_;
    std::map<std::string, std::vector<double>> history_;
};

/**
 * @brief RAII timer to measure scope execution time
 */
export class ScopedPerformanceTimer {
public:
    ScopedPerformanceTimer(const std::string& name) 
        : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~ScopedPerformanceTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start_;
        PerformanceRegistry::instance().recordSample(name_, elapsed.count());
    }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace ArtifactCore
