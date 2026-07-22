module;
#include <utility>
#include <cstdint>

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
        cpuUsage_ = 0.0f; // Requires more complex sampling; placeholder
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
};

}