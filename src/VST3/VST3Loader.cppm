module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <iostream>
#include <string>
#include <filesystem>

module VST3.Interfaces;

namespace Steinberg {

// ─────────────────────────────────────────────────────────
// VST3 モジュール読み込み
// ─────────────────────────────────────────────────────────
bool VST3Module::load(const std::string& path)
{
    if (!std::filesystem::exists(path)) {
        std::cerr << "[VST3] File not found: " << path << std::endl;
        return false;
    }

#ifdef _WIN32
    moduleHandle_ = LoadLibraryW(
        std::filesystem::path(path).wstring().c_str());
    if (!moduleHandle_) {
        std::cerr << "[VST3] LoadLibrary failed: " << GetLastError() << std::endl;
        return false;
    }
    getFactoryProc_ = reinterpret_cast<GetFactoryProc>(
        GetProcAddress(static_cast<HMODULE>(moduleHandle_), "GetPluginFactory"));
#else
    moduleHandle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!moduleHandle_) {
        std::cerr << "[VST3] dlopen failed: " << dlerror() << std::endl;
        return false;
    }
    getFactoryProc_ = reinterpret_cast<GetFactoryProc>(
        dlsym(moduleHandle_, "GetPluginFactory"));
#endif

    if (!getFactoryProc_) {
        std::cerr << "[VST3] GetPluginFactory not found" << std::endl;
        unload();
        return false;
    }

    factory_ = getFactoryProc_();
    if (!factory_) {
        std::cerr << "[VST3] GetPluginFactory returned null" << std::endl;
        unload();
        return false;
    }

    std::cout << "[VST3] Loaded: " << path
              << " (" << factory_->countPlugins() << " plugins)" << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────
// モジュール解放
// ─────────────────────────────────────────────────────────
void VST3Module::unload()
{
    factory_ = nullptr;
    getFactoryProc_ = nullptr;

    if (moduleHandle_) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(moduleHandle_));
#else
        dlclose(moduleHandle_);
#endif
        moduleHandle_ = nullptr;
    }
}

} // namespace Steinberg
