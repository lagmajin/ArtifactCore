module;
#include <utility>
#include <string>

export module OS;

export namespace ArtifactCore {

enum class OSFamily {
    Unknown,
    Windows,
    macOS,
    Linux
};

enum class Architecture {
    Unknown,
    x86_64,
    ARM64
};

class OSInfo {
public:
    OSInfo() {
#if defined(_WIN32) || defined(_WIN64)
        family_ = OSFamily::Windows;
        name_ = "Windows";
#elif defined(__APPLE__)
        family_ = OSFamily::macOS;
        name_ = "macOS";
#elif defined(__linux__)
        family_ = OSFamily::Linux;
        name_ = "Linux";
#else
        family_ = OSFamily::Unknown;
        name_ = "Unknown";
#endif

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
        arch_ = Architecture::x86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
        arch_ = Architecture::ARM64;
#else
        arch_ = Architecture::Unknown;
#endif
    }

    OSFamily family() const { return family_; }
    std::string name() const { return name_; }
    std::string version() const { return version_; }
    Architecture architecture() const { return arch_; }
    bool is64Bit() const { return arch_ == Architecture::x86_64 || arch_ == Architecture::ARM64; }

    static OSInfo current() { return OSInfo(); }

private:
    OSFamily family_ = OSFamily::Unknown;
    Architecture arch_ = Architecture::Unknown;
    std::string name_;
    std::string version_;
};

}