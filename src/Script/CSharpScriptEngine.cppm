module;

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <filesystem>

// .NET ホスティング型 — hostfxr SDK 非依存の自己定義
// coreclr_delegates.h / hostfxr.h が無くてもコンパイル可能にする
#ifdef ARTIFACT_HAS_DOTNET

// hostfxr 関数ポインタ型
using hostfxr_initialize_for_runtime_config_fn = int(__cdecl*)(
    const wchar_t* runtime_config_path,
    void* parameters,
    void** host_context_handle);

using hostfxr_get_runtime_delegate_fn = int(__cdecl*)(
    void* host_context_handle,
    int32_t type,
    void** delegate);

using hostfxr_close_fn = int(__cdecl*)(
    void* host_context_handle);

// coreclr_delegates.h 相当
using load_assembly_fn = int(__cdecl*)(
    const wchar_t* assembly_path,
    const wchar_t* assembly_name,
    void* load_assembly_property_keys,
    void* load_assembly_property_values);

using get_function_pointer_fn = int(__cdecl*)(
    const wchar_t* type_name,
    const wchar_t* method_name,
    const void* delegate_type_interface,
    const void* reserved,
    void* function_pointer_consumer,
    void** function_pointer);

// hdt_* delegate type enum (coreclr_delegates.h)
constexpr int32_t hdt_load_assembly = 1;
constexpr int32_t hdt_get_function_pointer = 3;
constexpr int32_t UNMANAGEDCALLERSONLY_METHOD = 0;

#endif // ARTIFACT_HAS_DOTNET

module Script.CSharp.Engine;

namespace ArtifactCore {

namespace fs = std::filesystem;

#ifdef ARTIFACT_HAS_DOTNET

// ─────────────────────────────────────────────────────────
// DotnetRuntimeHost — hostfxr 経由で .NET ランタイムをロード
// ─────────────────────────────────────────────────────────
class DotnetRuntimeHost {
public:
    DotnetRuntimeHost() = default;
    ~DotnetRuntimeHost() { shutdown(); }

    bool initialize(const std::string& dotnetRoot) {
        std::string fxrDir = dotnetRoot + "/host/fxr/";
        if (!fs::exists(fxrDir)) {
            lastError_ = "hostfxr directory not found: " + fxrDir;
            return false;
        }

        // 最新バージョンの hostfxr.dll を探す
        std::string latestDir;
        for (const auto& entry : fs::directory_iterator(fxrDir)) {
            if (!entry.is_directory()) continue;
            std::string dirName = entry.path().filename().string();
            if (latestDir.empty() || dirName > latestDir)
                latestDir = entry.path().string();
        }

        if (latestDir.empty()) {
            lastError_ = "No hostfxr version found";
            return false;
        }

#ifdef _WIN32
        std::wstring fxrPath = fs::path(latestDir + "/hostfxr.dll").wstring();
        libHandle_ = LoadLibraryW(fxrPath.c_str());
        if (!libHandle_) {
            lastError_ = "Failed to load hostfxr.dll";
            return false;
        }

        initRuntime_ = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            GetProcAddress(static_cast<HMODULE>(libHandle_), "hostfxr_initialize_for_runtime_config"));
        getDelegate_ = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            GetProcAddress(static_cast<HMODULE>(libHandle_), "hostfxr_get_runtime_delegate"));
        closeFn_ = reinterpret_cast<hostfxr_close_fn>(
            GetProcAddress(static_cast<HMODULE>(libHandle_), "hostfxr_close"));
#else
        // Linux/macOS: dlopen
        libHandle_ = dlopen((latestDir + "/libhostfxr.so").c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!libHandle_) {
            lastError_ = "Failed to load libhostfxr.so";
            return false;
        }
        initRuntime_ = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            dlsym(libHandle_, "hostfxr_initialize_for_runtime_config"));
        getDelegate_ = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            dlsym(libHandle_, "hostfxr_get_runtime_delegate"));
        closeFn_ = reinterpret_cast<hostfxr_close_fn>(
            dlsym(libHandle_, "hostfxr_close"));
#endif

        if (!initRuntime_ || !getDelegate_ || !closeFn_) {
            lastError_ = "Failed to resolve hostfxr functions";
            return false;
        }

        return true;
    }

    bool loadAssembly(const std::string& assemblyPath) {
        if (!initRuntime_ || !getDelegate_) {
            lastError_ = "hostfxr not initialized";
            return false;
        }

        fs::path dllPath(assemblyPath);
        if (!fs::exists(dllPath)) {
            lastError_ = "Assembly not found: " + assemblyPath;
            return false;
        }

        // runtimeconfig.json のパスをアセンブリパスから推測
        std::string configPath = dllPath.parent_path().string() + "/" +
                                 dllPath.stem().string() + ".runtimeconfig.json";
        if (!fs::exists(configPath)) {
            lastError_ = "runtimeconfig.json not found: " + configPath;
            return false;
        }

#ifdef _WIN32
        std::wstring configW = fs::path(configPath).wstring();
        std::wstring assemblyW = fs::path(assemblyPath).wstring();
#else
        std::wstring configW(configPath.begin(), configPath.end());
        std::wstring assemblyW(assemblyPath.begin(), assemblyPath.end());
#endif

        int rc = initRuntime_(configW.c_str(), nullptr, &hostContext_);
        if (rc != 0 || !hostContext_) {
            lastError_ = "hostfxr_initialize_for_runtime_config failed: " + std::to_string(rc);
            return false;
        }

        rc = getDelegate_(hostContext_, hdt_load_assembly,
                          reinterpret_cast<void**>(&loadAssembly_));
        rc = getDelegate_(hostContext_, hdt_get_function_pointer,
                          reinterpret_cast<void**>(&getFnPtr_));

        if (!loadAssembly_ || !getFnPtr_) {
            lastError_ = "Failed to get .NET runtime delegates";
            return false;
        }

        rc = loadAssembly_(assemblyW.c_str(), nullptr, nullptr, nullptr);
        if (rc != 0) {
            lastError_ = "load_assembly failed: " + std::to_string(rc);
            return false;
        }

        loaded_ = true;
        return true;
    }

    void* getFunctionPointer(const std::string& typeName, const std::string& methodName) {
        if (!getFnPtr_) return nullptr;
        void* fn = nullptr;

#ifdef _WIN32
        std::wstring typeW(typeName.begin(), typeName.end());
        std::wstring methodW(methodName.begin(), methodName.end());
#else
        std::wstring typeW(typeName.begin(), typeName.end());
        std::wstring methodW(methodName.begin(), methodName.end());
#endif

        int rc = getFnPtr_(typeW.c_str(), methodW.c_str(),
                           reinterpret_cast<const void*>(UNMANAGEDCALLERSONLY_METHOD),
                           nullptr, nullptr, &fn);
        return (rc == 0) ? fn : nullptr;
    }

    void shutdown() {
        if (hostContext_ && closeFn_)
            closeFn_(hostContext_);
        hostContext_ = nullptr;
#ifdef _WIN32
        if (libHandle_) FreeLibrary(static_cast<HMODULE>(libHandle_));
#else
        if (libHandle_) dlclose(libHandle_);
#endif
        libHandle_ = nullptr;
        loaded_ = false;
    }

    bool isLoaded() const { return loaded_; }
    std::string lastError() const { return lastError_; }

private:
    void* libHandle_ = nullptr;
    void* hostContext_ = nullptr;
    hostfxr_initialize_for_runtime_config_fn initRuntime_ = nullptr;
    hostfxr_get_runtime_delegate_fn getDelegate_ = nullptr;
    hostfxr_close_fn closeFn_ = nullptr;
    load_assembly_fn loadAssembly_ = nullptr;
    get_function_pointer_fn getFnPtr_ = nullptr;
    bool loaded_ = false;
    std::string lastError_;
};

#endif // ARTIFACT_HAS_DOTNET

// ─────────────────────────────────────────────────────────
// CSharpScriptEngine 実装
// ─────────────────────────────────────────────────────────

class CSharpScriptEngine::Impl {
public:
#ifdef ARTIFACT_HAS_DOTNET
    DotnetRuntimeHost host;
#endif
    bool initialized_ = false;
    std::string lastError_;
    OutputCallback outputCallback_;
    std::string scriptingAssemblyPath_;

    void setError(const std::string& msg) {
        lastError_ = msg;
        if (outputCallback_)
            outputCallback_(msg, true);
    }
};

CSharpScriptEngine::CSharpScriptEngine()
    : impl_(new Impl()) {}

CSharpScriptEngine::~CSharpScriptEngine() { finalize(); delete impl_; }

CSharpScriptEngine& CSharpScriptEngine::instance() {
    static CSharpScriptEngine engine;
    return engine;
}

bool CSharpScriptEngine::initialize(const std::string& dotnetRoot) {
    if (impl_->initialized_) return true;

#ifdef ARTIFACT_HAS_DOTNET
    std::string root = dotnetRoot.empty()
        ? "C:/Program Files/dotnet" // Windows default
        : dotnetRoot;

    if (!impl_->host.initialize(root)) {
        impl_->setError(impl_->host.lastError());
        return false;
    }
    impl_->initialized_ = true;
    return true;
#else
    impl_->setError("CSharpScriptEngine: built without ARTIFACT_HAS_DOTNET (stub)");
    return false;
#endif
}

void CSharpScriptEngine::finalize() {
#ifdef ARTIFACT_HAS_DOTNET
    impl_->host.shutdown();
#endif
    impl_->initialized_ = false;
    impl_->lastError_.clear();
}

bool CSharpScriptEngine::isInitialized() const {
    return impl_->initialized_;
}

bool CSharpScriptEngine::execute(const std::string& assemblyPath) {
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }

#ifdef ARTIFACT_HAS_DOTNET
    const bool loaded = impl_->host.loadAssembly(assemblyPath);
    if (loaded && std::filesystem::path(assemblyPath).stem() == "Artifact.Scripting") {
        impl_->scriptingAssemblyPath_ = assemblyPath;
    }
    return loaded;
#else
    impl_->setError("CSharpScriptEngine: built without ARTIFACT_HAS_DOTNET (stub)");
    return false;
#endif
}

bool CSharpScriptEngine::loadAssembly(const std::string& assemblyPath) {
    return execute(assemblyPath);
}

bool CSharpScriptEngine::executeScript(const std::string& code) {
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }
#ifdef ARTIFACT_HAS_DOTNET
    if (impl_->scriptingAssemblyPath_.empty()) {
        impl_->setError("CSharpScriptEngine: Artifact.Scripting.dll is not loaded");
        return false;
    }
    const std::string result = evaluate("Artifact.Scripting.ArtifactScriptHost", "EvaluateCode", code);
    if (result.empty() && hasError()) return false;
    return true;
#else
    (void)code;
    impl_->setError("CSharpScriptEngine: built without ARTIFACT_HAS_DOTNET (stub)");
    return false;
#endif
}

bool CSharpScriptEngine::executeScriptFile(const std::string& path) {
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }
#ifdef ARTIFACT_HAS_DOTNET
    if (!std::filesystem::exists(path)) {
        impl_->setError("CSharpScriptEngine: script file not found: " + path);
        return false;
    }
    const std::string result = evaluate("Artifact.Scripting.ArtifactScriptHost", "ExecuteFile", path);
    return !(result.empty() && hasError());
#else
    (void)path;
    impl_->setError("CSharpScriptEngine: built without ARTIFACT_HAS_DOTNET (stub)");
    return false;
#endif
}

bool CSharpScriptEngine::executeScriptWithImports(const std::string& code,
                                                  const std::vector<std::string>& imports) {
    std::string source;
    for (const auto& import : imports) {
        source += "using " + import + ";\n";
    }
    source += code;
    return executeScript(source);
}

std::string CSharpScriptEngine::evaluate(const std::string& typeName,
                                          const std::string& methodName,
                                          const std::string& argument) {
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return "";
    }

#ifdef ARTIFACT_HAS_DOTNET
    void* fn = impl_->host.getFunctionPointer(typeName, methodName);
    if (!fn) {
        impl_->setError("Failed to get function pointer for " + typeName + "." + methodName);
        return "";
    }

    // component_entry_point_fn シグネチャで呼び出し
    using EntryPointFn = int(__cdecl*)(const wchar_t*, const wchar_t*,
                                        const wchar_t*, const wchar_t*,
                                        void*, int);
    auto entryFn = reinterpret_cast<EntryPointFn>(fn);

    std::wstring typeW(typeName.begin(), typeName.end());
    std::wstring methodW(methodName.begin(), methodName.end());
    std::wstring argW(argument.begin(), argument.end());

    char resultBuf[4096] = {};
    entryFn(nullptr, typeW.c_str(), methodW.c_str(), argW.c_str(),
            resultBuf, sizeof(resultBuf));
    return std::string(resultBuf);
#else
    impl_->setError("CSharpScriptEngine: built without ARTIFACT_HAS_DOTNET (stub)");
    return "";
#endif
}

void CSharpScriptEngine::setOutputCallback(OutputCallback callback) {
    impl_->outputCallback_ = std::move(callback);
}

std::string CSharpScriptEngine::getLastError() const {
    return impl_->lastError_;
}

bool CSharpScriptEngine::hasError() const {
    return !impl_->lastError_.empty();
}

void CSharpScriptEngine::clearError() {
    impl_->lastError_.clear();
}

} // namespace ArtifactCore
