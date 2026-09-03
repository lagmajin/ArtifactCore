module;

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <cctype>
#include <cmath>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iterator>
#include <iomanip>
#include <limits>
#include <locale>
#include <QProcess>
#include <QString>

#ifdef _WIN32
#define ARTIFACT_CDECL __cdecl
using artifact_host_char_t = wchar_t;
#else
#define ARTIFACT_CDECL
using artifact_host_char_t = char;
#endif

// .NET ホスティング型 — hostfxr SDK 非依存の自己定義
// coreclr_delegates.h / hostfxr.h が無くてもコンパイル可能にする
#ifdef ARTIFACT_HAS_DOTNET

// hostfxr 関数ポインタ型
using hostfxr_initialize_for_runtime_config_fn = int(ARTIFACT_CDECL*)(
    const artifact_host_char_t* runtime_config_path,
    void* parameters,
    void** host_context_handle);

using hostfxr_get_runtime_delegate_fn = int(ARTIFACT_CDECL*)(
    void* host_context_handle,
    int32_t type,
    void** delegate);

using hostfxr_close_fn = int(ARTIFACT_CDECL*)(
    void* host_context_handle);

// coreclr_delegates.h 相当
using load_assembly_fn = int(ARTIFACT_CDECL*)(
    const artifact_host_char_t* assembly_path,
    void* load_context,
    void* reserved);

using get_function_pointer_fn = int(ARTIFACT_CDECL*)(
    const artifact_host_char_t* type_name,
    const artifact_host_char_t* method_name,
    const artifact_host_char_t* delegate_type_name,
    void* load_context,
    void* reserved,
    void** function_pointer);

// hdt_* delegate type enum (coreclr_delegates.h)
constexpr int32_t hdt_get_function_pointer = 6;
constexpr int32_t hdt_load_assembly = 7;

#endif // ARTIFACT_HAS_DOTNET

module Script.CSharp.Engine;

import Container.NamedVector;

namespace ArtifactCore {

namespace fs = std::filesystem;

namespace {

std::string makeSessionTimePayload(double timeSeconds,
                                   double deltaSeconds,
                                   std::uint64_t frame) {
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    payload << std::setprecision(std::numeric_limits<double>::max_digits10)
            << timeSeconds << ';' << deltaSeconds << ';' << frame;
    return payload.str();
}

std::array<int, 4> parseRuntimeVersion(const std::string& value) {
    std::array<int, 4> parts{};
    std::size_t begin = 0;
    for (std::size_t index = 0; index < parts.size() && begin < value.size(); ++index) {
        const std::size_t end = value.find('.', begin);
        const std::string component = value.substr(begin, end == std::string::npos
                                                             ? std::string::npos
                                                             : end - begin);
        std::size_t digits = 0;
        while (digits < component.size() &&
               std::isdigit(static_cast<unsigned char>(component[digits]))) {
            ++digits;
        }
        if (digits > 0)
            parts[index] = std::stoi(component.substr(0, digits));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return parts;
}

bool runtimeVersionGreater(const std::string& left, const std::string& right) {
    return parseRuntimeVersion(left) > parseRuntimeVersion(right);
}

} // namespace

#ifdef ARTIFACT_HAS_DOTNET

// ─────────────────────────────────────────────────────────
// DotnetRuntimeHost — hostfxr 経由で .NET ランタイムをロード
// ─────────────────────────────────────────────────────────
class DotnetRuntimeHost {
public:
    DotnetRuntimeHost() = default;
    ~DotnetRuntimeHost() { shutdown(); }

    bool initialize(const std::string& dotnetRoot) {
        dotnetRoot_ = dotnetRoot;
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
            const std::string latestVersion = latestDir.empty()
                ? std::string{}
                : fs::path(latestDir).filename().string();
            if (latestDir.empty() || runtimeVersionGreater(dirName, latestVersion))
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
            shutdown();
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

        fs::path configPath = dllPath.parent_path() /
                              (dllPath.stem().string() + ".runtimeconfig.json");
        if (!fs::exists(configPath)) {
            std::error_code ec;
            const fs::path executableConfig =
                fs::current_path(ec) / "Artifact.runtimeconfig.json";
            if (!ec && fs::exists(executableConfig)) {
                configPath = executableConfig;
            } else {
                std::error_code runtimeEc;
                const fs::path sharedRuntimeRoot = fs::path(dotnetRoot_) /
                    "shared/Microsoft.NETCore.App";
                std::string latestRuntime;
                if (fs::exists(sharedRuntimeRoot)) {
                    for (const auto& entry : fs::directory_iterator(sharedRuntimeRoot, runtimeEc)) {
                        if (runtimeEc || !entry.is_directory())
                            continue;
                        const std::string version = entry.path().filename().string();
                        if (latestRuntime.empty() || runtimeVersionGreater(version, latestRuntime))
                            latestRuntime = version;
                    }
                }
                if (latestRuntime.empty()) {
                    lastError_ = "runtimeconfig.json not found beside assembly: " +
                                 configPath.string();
                    return false;
                }

                configPath = dllPath.parent_path() /
                    (dllPath.stem().string() + ".runtimeconfig.json");
                std::ofstream generatedConfig(configPath, std::ios::trunc);
                if (!generatedConfig) {
                    std::error_code tempEc;
                    const fs::path tempRoot = fs::temp_directory_path(tempEc);
                    if (tempEc) {
                        lastError_ = "Unable to locate temporary directory for runtimeconfig.json";
                        return false;
                    }
                    const auto suffix = std::to_string(std::hash<std::string>{}(assemblyPath));
                    configPath = tempRoot / ("Artifact." + dllPath.stem().string() + "." +
                                             suffix + ".runtimeconfig.json");
                    generatedConfig.clear();
                    generatedConfig.open(configPath, std::ios::trunc);
                    if (!generatedConfig) {
                        lastError_ = "Unable to generate runtimeconfig.json: " + configPath.string();
                        return false;
                    }
                }
                generatedConfig << "{\n"
                    << "  \"runtimeOptions\": {\n"
                    << "    \"tfm\": \"net" << latestRuntime.substr(0, latestRuntime.find('.'))
                    << ".0\",\n"
                    << "    \"framework\": {\n"
                    << "      \"name\": \"Microsoft.NETCore.App\",\n"
                    << "      \"version\": \"" << latestRuntime << "\"\n"
                    << "    }\n"
                    << "  }\n"
                    << "}\n";
                if (!generatedConfig.good()) {
                    generatedConfig.close();
                    std::error_code removeEc;
                    fs::remove(configPath, removeEc);
                    lastError_ = "Unable to write generated runtimeconfig.json: " + configPath.string();
                    return false;
                }
                generatedConfig.close();
                generatedRuntimeConfigs_.add(configPath);
            }
        }

#ifdef _WIN32
        const auto assemblyNative = fs::path(assemblyPath).wstring();
#else
        const auto assemblyNative = fs::path(assemblyPath).string();
#endif
        if (hostContext_ && loadAssembly_ && getFnPtr_) {
            const int rc = loadAssembly_(assemblyNative.c_str(), nullptr, nullptr);
            if (rc != 0) {
                lastError_ = "load_assembly failed: " + std::to_string(rc);
                return false;
            }
            loaded_ = true;
            return true;
        }

#ifdef _WIN32
        auto configNative = configPath.wstring();
        auto configAssemblyNative = assemblyNative;
#else
        auto configNative = configPath.string();
        auto configAssemblyNative = assemblyNative;
#endif

        int rc = initRuntime_(configNative.c_str(), nullptr, &hostContext_);
        if (rc != 0 || !hostContext_) {
            lastError_ = "hostfxr_initialize_for_runtime_config failed: " + std::to_string(rc);
            return false;
        }

        rc = getDelegate_(hostContext_, hdt_load_assembly,
                          reinterpret_cast<void**>(&loadAssembly_));
        if (rc != 0 || !loadAssembly_) {
            lastError_ = "Failed to get load_assembly delegate: " + std::to_string(rc);
            resetRuntimeContext();
            return false;
        }
        rc = getDelegate_(hostContext_, hdt_get_function_pointer,
                          reinterpret_cast<void**>(&getFnPtr_));
        if (rc != 0 || !loadAssembly_ || !getFnPtr_) {
            if (rc != 0)
                lastError_ = "Failed to get get_function_pointer delegate: " + std::to_string(rc);
            else
                lastError_ = "Failed to get .NET runtime delegates";
            resetRuntimeContext();
            return false;
        }

        rc = loadAssembly_(configAssemblyNative.c_str(), nullptr, nullptr);
        if (rc != 0) {
            lastError_ = "load_assembly failed: " + std::to_string(rc);
            resetRuntimeContext();
            return false;
        }

        loaded_ = true;
        return true;
    }

    void* getFunctionPointer(const std::string& typeName, const std::string& methodName) {
        if (!getFnPtr_) return nullptr;
        void* fn = nullptr;

        auto typeNative = fs::path(typeName).native();
        auto methodNative = fs::path(methodName).native();

        const auto unmanagedCallerOnly = reinterpret_cast<const artifact_host_char_t*>(
            static_cast<std::intptr_t>(-1));
        int rc = getFnPtr_(typeNative.c_str(), methodNative.c_str(),
                           unmanagedCallerOnly,
                           nullptr, nullptr, &fn);
        if (rc != 0) {
            lastError_ = "get_function_pointer failed for " + typeName + "." + methodName +
                         ": " + std::to_string(rc);
            return nullptr;
        }
        return fn;
    }

    bool invokeUtf8(const std::string& typeName,
                    const std::string& methodName,
                    const std::string& argument,
                    std::string& result) {
        void* raw = getFunctionPointer(typeName, methodName);
        if (!raw) {
            lastError_ = "Failed to resolve native method: " + typeName + "." + methodName;
            return false;
        }

        using NativeEntryPoint = int(ARTIFACT_CDECL*)(const void*, void*, int);
        auto entryPoint = reinterpret_cast<NativeEntryPoint>(raw);
        std::vector<char> buffer(64 * 1024, '\0');
        const int rc = entryPoint(argument.data(), buffer.data(),
                                  static_cast<int>(buffer.size()));
        result.assign(buffer.data());
        if (rc != 0) {
            lastError_ = result.empty()
                ? "Native C# method failed: " + std::to_string(rc)
                : "Native C# method failed: " + result;
            return false;
        }
        return true;
    }

    void shutdown() {
        resetRuntimeContext();
#ifdef _WIN32
        if (libHandle_) FreeLibrary(static_cast<HMODULE>(libHandle_));
#else
        if (libHandle_) dlclose(libHandle_);
#endif
        libHandle_ = nullptr;
        initRuntime_ = nullptr;
        getDelegate_ = nullptr;
        closeFn_ = nullptr;
        loadAssembly_ = nullptr;
        getFnPtr_ = nullptr;
        loaded_ = false;
        for (const auto& generatedConfig : generatedRuntimeConfigs_) {
            std::error_code ec;
            fs::remove(generatedConfig, ec);
        }
        generatedRuntimeConfigs_.clear();
    }

    bool isLoaded() const { return loaded_; }
    std::string lastError() const { return lastError_; }

private:
    void resetRuntimeContext() {
        if (hostContext_ && closeFn_)
            closeFn_(hostContext_);
        hostContext_ = nullptr;
        loadAssembly_ = nullptr;
        getFnPtr_ = nullptr;
    }

    void* libHandle_ = nullptr;
    void* hostContext_ = nullptr;
    hostfxr_initialize_for_runtime_config_fn initRuntime_ = nullptr;
    hostfxr_get_runtime_delegate_fn getDelegate_ = nullptr;
    hostfxr_close_fn closeFn_ = nullptr;
    load_assembly_fn loadAssembly_ = nullptr;
    get_function_pointer_fn getFnPtr_ = nullptr;
    bool loaded_ = false;
    std::string lastError_;
    std::string dotnetRoot_;
    NamedVector<fs::path> generatedRuntimeConfigs_;
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
    bool externalRuntime_ = false;
    std::string externalExecutable_ = "dotnet";
    std::string scriptHostAssemblyPath_;
    bool scriptSessionActive_ = false;
    fs::path scriptSessionSourcePath_;
    fs::file_time_type scriptSessionSourceTime_{};
    bool hasScriptSessionSourceTime_ = false;
    double scriptSessionTimeSeconds_ = 0.0;
    double scriptSessionDeltaSeconds_ = 0.0;
    std::uint64_t scriptSessionFrame_ = 0;
    bool scriptSessionStopRequested_ = false;
    std::size_t scriptSessionSourceHash_ = 0;
    bool hasScriptSessionSourceHash_ = false;
    mutable std::recursive_mutex scriptSessionMutex_;

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

void CSharpScriptEngine::setScriptHostAssemblyPath(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    impl_->scriptHostAssemblyPath_ = path;
}

bool CSharpScriptEngine::initialize(const std::string& dotnetRoot) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (impl_->initialized_) return true;

#ifdef ARTIFACT_HAS_DOTNET
    std::string root = dotnetRoot;
    if (root.empty()) {
        if (const char* envRoot = std::getenv("DOTNET_ROOT"); envRoot && *envRoot)
            root = envRoot;
    }
    if (root.empty()) {
#ifdef _WIN32
        root = "C:/Program Files/dotnet";
#elif defined(__APPLE__)
        root = "/usr/local/share/dotnet";
#else
        root = "/usr/share/dotnet";
        if (!fs::exists(root))
            root = "/usr/lib/dotnet";
#endif
    }

    if (!impl_->host.initialize(root)) {
        impl_->setError(impl_->host.lastError());
        return false;
    }
    impl_->initialized_ = true;
    return true;
#else
    std::string executablePath = dotnetRoot;
    if (!executablePath.empty() && fs::is_directory(executablePath)) {
#ifdef _WIN32
        executablePath += "/dotnet.exe";
#else
        executablePath += "/dotnet";
#endif
    }
    const QString executable = executablePath.empty()
        ? QStringLiteral("dotnet") : QString::fromStdString(executablePath);
    QProcess probe;
    probe.setProgram(executable);
    probe.setArguments({QStringLiteral("--version")});
    probe.start();
    if (!probe.waitForStarted(2000) || !probe.waitForFinished(5000) ||
        probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        impl_->setError("CSharpScriptEngine: .NET runtime was not found");
        return false;
    }
    impl_->externalExecutable_ = executable.toStdString();
    impl_->externalRuntime_ = true;
    impl_->initialized_ = true;
    impl_->lastError_.clear();
    return true;
#endif
}

void CSharpScriptEngine::finalize() {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    impl_->host.shutdown();
#endif
    impl_->initialized_ = false;
    impl_->externalRuntime_ = false;
    impl_->scriptSessionActive_ = false;
    impl_->scriptSessionSourcePath_.clear();
    impl_->hasScriptSessionSourceTime_ = false;
    impl_->scriptSessionTimeSeconds_ = 0.0;
    impl_->scriptSessionDeltaSeconds_ = 0.0;
    impl_->scriptSessionFrame_ = 0;
    impl_->scriptSessionStopRequested_ = false;
    impl_->scriptSessionSourceHash_ = 0;
    impl_->hasScriptSessionSourceHash_ = false;
    impl_->lastError_.clear();
}

bool CSharpScriptEngine::isInitialized() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    return impl_->initialized_;
}

bool CSharpScriptEngine::execute(const std::string& assemblyPath) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }

#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->host.loadAssembly(assemblyPath)) {
        impl_->setError(impl_->host.lastError());
        return false;
    }
    return true;
#else
    if (!impl_->externalRuntime_) return false;
    QProcess process;
    process.setProgram(QString::fromStdString(impl_->externalExecutable_));
    process.setArguments({QString::fromStdString(assemblyPath)});
    process.start();
    if (!process.waitForStarted(2000) || !process.waitForFinished(30000)) {
        impl_->setError("CSharpScriptEngine: external process did not finish");
        process.kill();
        return false;
    }
    const QByteArray output = process.readAllStandardOutput();
    const QByteArray error = process.readAllStandardError();
    if (!output.isEmpty() && impl_->outputCallback_)
        impl_->outputCallback_(output.toStdString(), false);
    if (!error.isEmpty() && impl_->outputCallback_)
        impl_->outputCallback_(error.toStdString(), true);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        impl_->setError(error.isEmpty() ? "CSharpScriptEngine: assembly execution failed"
                                        : error.toStdString());
        return false;
    }
    return true;
#endif
}

bool CSharpScriptEngine::loadAssembly(const std::string& assemblyPath) {
    return execute(assemblyPath);
}

bool CSharpScriptEngine::executeScript(const std::string& code) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }

    const std::array<fs::path, 3> candidates = {
        fs::current_path() / "Artifact.Scripting.dll",
        fs::current_path() / "scripts/dotnet/Artifact.Scripting/bin/Release/net8.0/Artifact.Scripting.dll",
        fs::current_path() / "scripts/dotnet/Artifact.Scripting/bin/Debug/net8.0/Artifact.Scripting.dll"};
    fs::path hostAssembly = impl_->scriptHostAssemblyPath_.empty()
        ? fs::path{}
        : fs::path(impl_->scriptHostAssemblyPath_);
    if (hostAssembly.empty() || !fs::exists(hostAssembly)) {
        hostAssembly.clear();
        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) {
                hostAssembly = candidate;
                break;
            }
        }
    }
    if (hostAssembly.empty() || !impl_->host.loadAssembly(hostAssembly.string())) {
        impl_->setError(hostAssembly.empty()
            ? "CSharpScriptEngine: Artifact.Scripting.dll was not found"
            : impl_->host.lastError());
        return false;
    }

    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateCode", code, result)) {
        impl_->setError(impl_->host.lastError());
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    return true;
#else
    impl_->setError("CSharpScriptEngine: CSX requires embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::executeScriptFile(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    std::ifstream file(path);
    if (!file) {
        impl_->setError("CSharpScriptEngine: CSX file not found: " + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (file.bad()) {
        impl_->setError("CSharpScriptEngine: failed to read script source: " + path);
        return false;
    }
    return executeScript(contents.str());
}

bool CSharpScriptEngine::executeScriptWithImports(
    const std::string& code, const std::vector<std::string>& imports) {
    std::string source;
    for (const auto& importName : imports) {
        if (!importName.empty())
            source += "using " + importName + ";\n";
    }
    source += code;
    return executeScript(source);
}

bool CSharpScriptEngine::beginScriptSession(const std::string& code) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return false;
    }
    if (impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is already active");
        return false;
    }
    if (code.empty()) {
        impl_->setError("CSharpScriptEngine: session bootstrap code is empty");
        return false;
    }
    const std::array<fs::path, 3> candidates = {
        fs::current_path() / "Artifact.Scripting.dll",
        fs::current_path() / "scripts/dotnet/Artifact.Scripting/bin/Release/net8.0/Artifact.Scripting.dll",
        fs::current_path() / "scripts/dotnet/Artifact.Scripting/bin/Debug/net8.0/Artifact.Scripting.dll"};
    fs::path hostAssembly = impl_->scriptHostAssemblyPath_.empty()
        ? fs::path{}
        : fs::path(impl_->scriptHostAssemblyPath_);
    if (hostAssembly.empty() || !fs::exists(hostAssembly)) {
        hostAssembly.clear();
        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) {
                hostAssembly = candidate;
                break;
            }
        }
    }
    if (hostAssembly.empty() || !impl_->host.loadAssembly(hostAssembly.string())) {
        impl_->setError(hostAssembly.empty()
            ? "CSharpScriptEngine: Artifact.Scripting.dll was not found"
            : impl_->host.lastError());
        return false;
    }
    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "ResetSession", "", result)) {
        impl_->setError(impl_->host.lastError());
        return false;
    }
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateSession", code, result)) {
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->scriptSessionActive_ = true;
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::stepScriptSession(const std::string& code) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }
    if (impl_->scriptSessionStopRequested_) {
        impl_->setError("CSharpScriptEngine: script session stop requested");
        return false;
    }
    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateSession", code, result)) {
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::beginScriptSessionFile(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (path.empty()) {
        impl_->setError("CSharpScriptEngine: script source path is empty");
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        impl_->setError("CSharpScriptEngine: script source file not found: " + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (file.bad()) {
        impl_->setError("CSharpScriptEngine: failed to read script source: " + path);
        return false;
    }
    const std::string code = contents.str();

    std::error_code ec;
    const fs::path sourcePath(path);
    const auto sourceTime = fs::last_write_time(sourcePath, ec);
    if (ec) {
        impl_->setError("CSharpScriptEngine: failed to stat script source: " + path);
        return false;
    }
    if (!beginScriptSession(code))
        return false;

    impl_->scriptSessionSourcePath_ = sourcePath;
    impl_->scriptSessionSourceTime_ = sourceTime;
    impl_->hasScriptSessionSourceTime_ = true;
    impl_->scriptSessionSourceHash_ = std::hash<std::string>{}(code);
    impl_->hasScriptSessionSourceHash_ = true;
    impl_->lastError_.clear();
    return true;
}

bool CSharpScriptEngine::updateScriptSession(const std::string& code,
                                             double timeSeconds,
                                             double deltaSeconds,
                                             std::uint64_t frame) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!std::isfinite(timeSeconds) || !std::isfinite(deltaSeconds)) {
        impl_->setError("CSharpScriptEngine: session time and delta must be finite");
        return false;
    }
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }
    if (impl_->scriptSessionStopRequested_) {
        impl_->setError("CSharpScriptEngine: script session stop requested");
        return false;
    }
    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "SetSessionTime",
                                makeSessionTimePayload(timeSeconds, deltaSeconds, frame),
                                result) ||
        !impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateSession", code, result)) {
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->scriptSessionTimeSeconds_ = timeSeconds;
    impl_->scriptSessionDeltaSeconds_ = deltaSeconds;
    impl_->scriptSessionFrame_ = frame;
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::updateScriptSessionFile(const std::string& path,
                                                 double timeSeconds,
                                                 double deltaSeconds,
                                                 std::uint64_t frame) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (!std::isfinite(timeSeconds) || !std::isfinite(deltaSeconds)) {
        impl_->setError("CSharpScriptEngine: session time and delta must be finite");
        return false;
    }
    if (path.empty()) {
        impl_->setError("CSharpScriptEngine: script source path is empty");
        return false;
    }
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        impl_->setError("CSharpScriptEngine: script source file not found: " + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (file.bad()) {
        impl_->setError("CSharpScriptEngine: failed to read script source: " + path);
        return false;
    }
    const std::string code = contents.str();

    const bool sourcePathChanged = impl_->scriptSessionSourcePath_.empty() ||
                                   impl_->scriptSessionSourcePath_ != fs::path(path);
    const bool sourceChanged = sourcePathChanged || isScriptSessionSourceChanged();
    if (sourceChanged) {
#ifdef ARTIFACT_HAS_DOTNET
        std::string result;
        if (!impl_->host.invokeUtf8("ArtifactScriptHost", "SetSessionTime",
                                    makeSessionTimePayload(timeSeconds, deltaSeconds, frame),
                                    result)) {
            impl_->setError(result.empty() ? impl_->host.lastError() : result);
            return false;
        }
#endif
        if (!reloadScriptSessionFile(path))
            return false;
        impl_->scriptSessionTimeSeconds_ = timeSeconds;
        impl_->scriptSessionDeltaSeconds_ = deltaSeconds;
        impl_->scriptSessionFrame_ = frame;
        impl_->lastError_.clear();
        return true;
    }

    return updateScriptSession(code, timeSeconds, deltaSeconds, frame);
}

bool CSharpScriptEngine::tickScriptSession(double timeSeconds,
                                           double deltaSeconds,
                                           std::uint64_t frame) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!std::isfinite(timeSeconds) || !std::isfinite(deltaSeconds)) {
        impl_->setError("CSharpScriptEngine: session time and delta must be finite");
        return false;
    }
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }
    if (impl_->scriptSessionStopRequested_) {
        impl_->setError("CSharpScriptEngine: script session stop requested");
        return false;
    }

    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "SetSessionTime",
                                makeSessionTimePayload(timeSeconds, deltaSeconds, frame),
                                result) ||
        !impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateSession",
                                "Update()", result)) {
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->scriptSessionTimeSeconds_ = timeSeconds;
    impl_->scriptSessionDeltaSeconds_ = deltaSeconds;
    impl_->scriptSessionFrame_ = frame;
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::invokeScriptSessionCallback(const std::string& functionName) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }
    if (impl_->scriptSessionStopRequested_) {
        impl_->setError("CSharpScriptEngine: script session stop requested");
        return false;
    }
    if (functionName.empty()) {
        impl_->setError("CSharpScriptEngine: callback name is empty");
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(functionName.front())) ||
          functionName.front() == '_')) {
        impl_->setError("CSharpScriptEngine: callback name is not a valid identifier");
        return false;
    }
    for (const char ch : functionName) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            impl_->setError("CSharpScriptEngine: callback name contains invalid characters");
            return false;
        }
    }
    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "EvaluateSession",
                                functionName + "()", result)) {
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::reloadScriptSession(const std::string& code) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (!impl_->scriptSessionActive_) {
        impl_->setError("CSharpScriptEngine: script session is not active");
        return false;
    }
    if (impl_->scriptSessionStopRequested_) {
        impl_->setError("CSharpScriptEngine: script session stop requested");
        return false;
    }
    std::string result;
    if (!impl_->host.invokeUtf8("ArtifactScriptHost", "ReloadSession", code, result)) {
        // ReloadSession only swaps the managed state after successful
        // evaluation, so a compilation/runtime failure leaves the previous
        // session usable.
        impl_->setError(result.empty() ? impl_->host.lastError() : result);
        return false;
    }
    if (impl_->outputCallback_ && !result.empty())
        impl_->outputCallback_(result, false);
    impl_->lastError_.clear();
    return true;
#else
    impl_->setError("CSharpScriptEngine: script sessions require embedded hostfxr support");
    return false;
#endif
}

bool CSharpScriptEngine::reloadScriptSessionFile(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (path.empty()) {
        impl_->setError("CSharpScriptEngine: script source path is empty");
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        impl_->setError("CSharpScriptEngine: script source file not found: " + path);
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (file.bad()) {
        impl_->setError("CSharpScriptEngine: failed to read script source: " + path);
        return false;
    }
    const std::string code = contents.str();
    std::error_code ec;
    const fs::path sourcePath(path);
    const auto sourceTime = fs::last_write_time(sourcePath, ec);
    if (ec) {
        impl_->setError("CSharpScriptEngine: failed to stat script source: " + path);
        return false;
    }
    if (!reloadScriptSession(code))
        return false;

    impl_->scriptSessionSourcePath_ = sourcePath;
    impl_->scriptSessionSourceTime_ = sourceTime;
    impl_->hasScriptSessionSourceTime_ = true;
    impl_->scriptSessionSourceHash_ = std::hash<std::string>{}(code);
    impl_->hasScriptSessionSourceHash_ = true;
    impl_->lastError_.clear();
    return true;
}

bool CSharpScriptEngine::isScriptSessionSourceChanged() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (!impl_->scriptSessionActive_ ||
        !impl_->hasScriptSessionSourceTime_ ||
        impl_->scriptSessionSourcePath_.empty()) {
        return false;
    }
    std::ifstream file(impl_->scriptSessionSourcePath_, std::ios::binary);
    if (!file)
        return true;
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    if (file.bad())
        return true;
    if (impl_->hasScriptSessionSourceHash_ &&
        std::hash<std::string>{}(contents) != impl_->scriptSessionSourceHash_) {
        return true;
    }
    std::error_code ec;
    const auto currentTime = fs::last_write_time(impl_->scriptSessionSourcePath_, ec);
    // A deleted or temporarily inaccessible source must not look unchanged;
    // the caller can attempt a reload and retain the previous ScriptState on
    // failure.
    return ec || currentTime != impl_->scriptSessionSourceTime_;
}

void CSharpScriptEngine::endScriptSession() {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
#ifdef ARTIFACT_HAS_DOTNET
    if (impl_->scriptSessionActive_) {
        std::string ignored;
        impl_->host.invokeUtf8("ArtifactScriptHost", "ResetSession", "", ignored);
    }
#endif
    impl_->scriptSessionActive_ = false;
    impl_->scriptSessionStopRequested_ = false;
    impl_->scriptSessionSourcePath_.clear();
    impl_->hasScriptSessionSourceTime_ = false;
    impl_->scriptSessionTimeSeconds_ = 0.0;
    impl_->scriptSessionDeltaSeconds_ = 0.0;
    impl_->scriptSessionFrame_ = 0;
    impl_->scriptSessionSourceHash_ = 0;
    impl_->hasScriptSessionSourceHash_ = false;
}

bool CSharpScriptEngine::isScriptSessionActive() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    return impl_->scriptSessionActive_;
}

void CSharpScriptEngine::requestScriptSessionStop() {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (impl_->scriptSessionActive_)
        impl_->scriptSessionStopRequested_ = true;
}

void CSharpScriptEngine::clearScriptSessionStopRequest() {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (impl_->scriptSessionActive_)
        impl_->scriptSessionStopRequested_ = false;
}

bool CSharpScriptEngine::isScriptSessionStopRequested() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    return impl_->scriptSessionStopRequested_;
}

ScriptSessionSnapshot CSharpScriptEngine::scriptSessionSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    ScriptSessionSnapshot snapshot;
    snapshot.active = impl_->scriptSessionActive_;
    snapshot.stopRequested = impl_->scriptSessionStopRequested_;
    snapshot.sourcePath = impl_->scriptSessionSourcePath_.string();
    snapshot.timeSeconds = impl_->scriptSessionTimeSeconds_;
    snapshot.deltaSeconds = impl_->scriptSessionDeltaSeconds_;
    snapshot.frame = impl_->scriptSessionFrame_;
    snapshot.lastError = impl_->lastError_;
    return snapshot;
}

std::string CSharpScriptEngine::evaluate(const std::string& typeName,
                                          const std::string& methodName,
                                          const std::string& argument) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    if (!impl_->initialized_) {
        impl_->setError("CSharpScriptEngine: not initialized");
        return "";
    }

#ifdef ARTIFACT_HAS_DOTNET
    std::string result;
    if (!impl_->host.invokeUtf8(typeName, methodName, argument, result)) {
        impl_->setError(impl_->host.lastError());
        return "";
    }
    return result;
#else
    impl_->setError("CSharpScriptEngine: method evaluation requires the embedded hostfxr runtime");
    return "";
#endif
}

void CSharpScriptEngine::setOutputCallback(OutputCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    impl_->outputCallback_ = std::move(callback);
}

std::string CSharpScriptEngine::getLastError() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    return impl_->lastError_;
}

bool CSharpScriptEngine::hasError() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    return !impl_->lastError_.empty();
}

void CSharpScriptEngine::clearError() {
    std::lock_guard<std::recursive_mutex> lock(impl_->scriptSessionMutex_);
    impl_->lastError_.clear();
}

} // namespace ArtifactCore
