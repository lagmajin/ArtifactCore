module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

module CLAP.Host;

import CLAP.Host;

namespace clap {

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────
// CLAP プラグイン動的ライブラリラッパー
// ─────────────────────────────────────────────────────────
struct PluginLibrary {
    void* handle = nullptr;
    std::string path;

    ~PluginLibrary() {
        if (handle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }
    }

    bool load(const std::string& path) {
#ifdef _WIN32
        handle = LoadLibraryW(fs::path(path).wstring().c_str());
        if (!handle) return false;
        PluginEntryProc entry = reinterpret_cast<PluginEntryProc>(
            GetProcAddress(static_cast<HMODULE>(handle), "clap_entry"));
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) return false;
        PluginEntryProc entry = reinterpret_cast<PluginEntryProc>(
            dlsym(handle, "clap_entry"));
#endif
        return entry != nullptr;
    }
};

// ─────────────────────────────────────────────────────────
// Host 実装
// ─────────────────────────────────────────────────────────
class Host::Impl {
public:
    std::vector<std::string> searchPaths;
    std::vector<std::unique_ptr<PluginLibrary>> libraries;
};

Host::Host() : impl_(new Impl()) {
#ifdef _WIN32
    impl_->searchPaths = {
        "C:/Program Files/Common Files/CLAP",
        "C:/Program Files/Common Files/VST3",
    };
#elif __APPLE__
    impl_->searchPaths = {
        "/Library/Audio/Plug-Ins/CLAP",
        "~/Library/Audio/Plug-Ins/CLAP",
    };
#else
    impl_->searchPaths = {
        "/usr/lib/clap",
        "/usr/local/lib/clap",
        "~/.clap",
    };
#endif
}

Host::~Host() { unloadAll(); }

void Host::addSearchPath(const std::string& path) {
    impl_->searchPaths.push_back(path);
}

void Host::setSearchPaths(const std::vector<std::string>& paths) {
    impl_->searchPaths = paths;
}

Plugin* Host::loadPlugin(const std::string& path) {
    if (!fs::exists(path)) {
        std::cerr << "[CLAP] File not found: " << path << std::endl;
        return nullptr;
    }

    auto lib = std::make_unique<PluginLibrary>();
    if (!lib->load(path)) {
        std::cerr << "[CLAP] Failed to load: " << path << std::endl;
        return nullptr;
    }

    std::cout << "[CLAP] Loaded: " << path << std::endl;
    impl_->libraries.push_back(std::move(lib));
    // Plugin instance creation requires the actual CLAP C API
    return nullptr; // 骨格: 実際の Plugin* は CLAP SDK 統合時に
}

void Host::unloadPlugin(Plugin* plugin) {
    // TODO: remove from plugins_, destroy plugin
    (void)plugin;
}

void Host::unloadAll() {
    for (auto* p : plugins_) {
        p->destroy();
        delete p;
    }
    plugins_.clear();
    impl_->libraries.clear();
}

std::vector<std::string> Host::scanPlugins() {
    std::vector<std::string> found;
    for (const auto& searchPath : impl_->searchPaths) {
        try {
            if (!fs::exists(searchPath)) continue;
            for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                if (entry.path().extension() == ".clap" ||
                    entry.path().extension() == ".dll" ||
                    entry.path().extension() == ".so") {
                    found.push_back(entry.path().string());
                }
            }
        } catch (...) {}
    }
    return found;
}

} // namespace clap
