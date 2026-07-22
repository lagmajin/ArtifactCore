module;
#include <utility>
#include <filesystem>
#include <string>
#include <cstdint>
#include <iomanip>
#include <sstream>

export module Disk.Size;

export namespace ArtifactCore {

class DiskSize {
public:
    DiskSize() = default;

    explicit DiskSize(const std::filesystem::path& path) {
        query(path);
    }

    void query(const std::filesystem::path& path) {
        std::error_code ec;
        auto info = std::filesystem::space(path, ec);
        if (!ec) {
            total_ = info.capacity;
            free_ = info.free;
            available_ = info.available;
            valid_ = true;
        } else {
            total_ = 0;
            free_ = 0;
            available_ = 0;
            valid_ = false;
        }
    }

    uint64_t totalBytes() const { return total_; }
    uint64_t freeBytes() const { return free_; }
    uint64_t availableBytes() const { return available_; }
    uint64_t usedBytes() const { return total_ > free_ ? total_ - free_ : 0; }

    std::string totalFormatted() const { return formatBytes(total_); }
    std::string freeFormatted() const { return formatBytes(free_); }
    std::string availableFormatted() const { return formatBytes(available_); }

    bool isValid() const { return valid_; }

    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unit = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            ++unit;
        }
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
        return ss.str();
    }

private:
    uint64_t total_ = 0;
    uint64_t free_ = 0;
    uint64_t available_ = 0;
    bool valid_ = false;
};

}