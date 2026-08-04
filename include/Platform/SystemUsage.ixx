module;
#include <utility>
#include <cstdint>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

export module SystemUsage;

export namespace ArtifactCore {

class SystemUsage {
public:
    SystemUsage() = default;

    // Memory in bytes
    uint64_t totalPhysicalMemory() const { return totalPhys_; }
    uint64_t availablePhysicalMemory() const { return availPhys_; }
    uint64_t usedPhysicalMemory() const { return totalPhys_ > availPhys_ ? totalPhys_ - availPhys_ : 0; }

    float memoryUsageRatio() const {
        if (totalPhys_ == 0) return 0.0f;
        return static_cast<float>(usedPhysicalMemory()) / static_cast<float>(totalPhys_);
    }

    float cpuUsageRatio() const { return cpuUsage_; }

    void update() {
#if defined(_WIN32) || defined(_WIN64)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            totalPhys_ = memInfo.ullTotalPhys;
            availPhys_ = memInfo.ullAvailPhys;
        }

        FILETIME idleTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            const auto toTicks = [](const FILETIME& value) -> uint64_t {
                return (static_cast<uint64_t>(value.dwHighDateTime) << 32) |
                       static_cast<uint64_t>(value.dwLowDateTime);
            };
            const uint64_t idle = toTicks(idleTime);
            const uint64_t kernel = toTicks(kernelTime);
            const uint64_t user = toTicks(userTime);
            if (hasCpuSample_) {
                const uint64_t idleDelta = idle - previousIdleTicks_;
                const uint64_t totalDelta = (kernel - previousKernelTicks_) +
                                             (user - previousUserTicks_);
                cpuUsage_ = totalDelta > 0
                    ? std::clamp(static_cast<float>(totalDelta - idleDelta) /
                                 static_cast<float>(totalDelta), 0.0f, 1.0f)
                    : cpuUsage_;
            }
            previousIdleTicks_ = idle;
            previousKernelTicks_ = kernel;
            previousUserTicks_ = user;
            hasCpuSample_ = true;
        }
#else
        totalPhys_ = 0;
        availPhys_ = 0;
        cpuUsage_ = 0.0f;
#endif
    }

private:
    uint64_t totalPhys_ = 0;
    uint64_t availPhys_ = 0;
    float cpuUsage_ = 0.0f;
#if defined(_WIN32) || defined(_WIN64)
    uint64_t previousIdleTicks_ = 0;
    uint64_t previousKernelTicks_ = 0;
    uint64_t previousUserTicks_ = 0;
    bool hasCpuSample_ = false;
#endif
};

}
