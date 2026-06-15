module;
// AngelScript SDK headers. Include only in the global module fragment;
// never place #include after the `module X;` declaration (AGENTS.md).
#ifdef ARTIFACT_HAS_ANGELSCRIPT
#include <angelscript.h>
// addon: std::string support, so scripts can call log("...") with strings.
#include <scriptstdstring/scriptstdstring.h>
#endif

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iostream>

module Script.AngelScript.Engine;

namespace ArtifactCore {

// ============================================================================
// Implementation
// ============================================================================

class AngelScriptEngine::Impl {
public:
    bool initialized_ = false;
    std::string lastError_;
    OutputCallback outputCallback_;
    mutable std::mutex mutex_;

#ifdef ARTIFACT_HAS_ANGELSCRIPT
    asIScriptEngine* engine_ = nullptr;
#endif

    Impl() {}
    ~Impl() {}

    void captureOutput(const std::string& text, bool isError) {
        if (outputCallback_) {
            outputCallback_(text, isError);
        } else {
            // Default: route to console so output is never silently lost.
            if (isError) {
                std::cerr << text;
            } else {
                std::cout << text;
            }
        }
    }

    void setError(const std::string& err) {
        lastError_ = err;
        captureOutput(err, true);
    }
};

// ============================================================================
// Singleton
// ============================================================================

AngelScriptEngine::AngelScriptEngine() : impl_(new Impl()) {}

AngelScriptEngine::~AngelScriptEngine() {
    finalize();
    delete impl_;
}

AngelScriptEngine& AngelScriptEngine::instance() {
    static AngelScriptEngine instance;
    return instance;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool AngelScriptEngine::initialize() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->initialized_) return true;

#ifdef ARTIFACT_HAS_ANGELSCRIPT
    impl_->engine_ = asCreateScriptEngine();
    if (!impl_->engine_) {
        impl_->setError("AngelScriptEngine: asCreateScriptEngine() returned null");
        return false;
    }

    // Register std::string support so scripts can use `string`.
    RegisterStdString(impl_->engine_);

    // Register log(string) / print(string). The callback routes text through
    // the singleton's OutputCallback. We store the Impl pointer as engine
    // user data so the generic callback can reach it without recursion into
    // the locked singleton API.
    impl_->engine_->SetUserData(impl_);

    int r = 0;
    r = impl_->engine_->RegisterGlobalFunction(
        "void log(const string &in)",
        asFUNCTION(+[](asIScriptGeneric* gen) {
            Impl* self = static_cast<Impl*>(gen->GetEngine()->GetUserData());
            std::string* s = static_cast<std::string*>(gen->GetAddressOfArg(0));
            std::string line = s ? *s : std::string();
            if (line.empty() || line.back() != '\n') line.push_back('\n');
            if (self) self->captureOutput(line, false);
        }),
        asCALL_GENERIC);
    if (r < 0) {
        impl_->setError(std::string("AngelScriptEngine: register log() failed: ") +
                        std::to_string(r));
    }

    r = impl_->engine_->RegisterGlobalFunction(
        "void print(const string &in)",
        asFUNCTION(+[](asIScriptGeneric* gen) {
            Impl* self = static_cast<Impl*>(gen->GetEngine()->GetUserData());
            std::string* s = static_cast<std::string*>(gen->GetAddressOfArg(0));
            std::string line = s ? *s : std::string();
            if (self) self->captureOutput(line, false);
        }),
        asCALL_GENERIC);
    if (r < 0) {
        impl_->setError(std::string("AngelScriptEngine: register print() failed: ") +
                        std::to_string(r));
    }

    impl_->initialized_ = true;
    return true;
#else
    impl_->setError("AngelScriptEngine: built without ARTIFACT_HAS_ANGELSCRIPT (stub)");
    return false;
#endif
}

void AngelScriptEngine::finalize() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
#ifdef ARTIFACT_HAS_ANGELSCRIPT
    if (impl_->engine_) {
        impl_->engine_->ShutDownAndRelease();
        impl_->engine_ = nullptr;
    }
#endif
    impl_->initialized_ = false;
}

bool AngelScriptEngine::isInitialized() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->initialized_;
}

// ============================================================================
// Output Capture
// ============================================================================

void AngelScriptEngine::setOutputCallback(OutputCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->outputCallback_ = std::move(callback);
}

// ============================================================================
// Compilation & Execution
// ============================================================================

bool AngelScriptEngine::compileModule(const std::string& moduleName,
                                      const std::string& source) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->lastError_.clear();

#ifdef ARTIFACT_HAS_ANGELSCRIPT
    if (!impl_->engine_) {
        impl_->setError("AngelScriptEngine: engine not initialized");
        return false;
    }

    asIScriptModule* mod = impl_->engine_->GetModule(moduleName.c_str(),
                                                     asGM_ALWAYS_CREATE);
    if (!mod) {
        impl_->setError("AngelScriptEngine: GetModule failed for " + moduleName);
        return false;
    }

    int r = mod->AddScriptSection("script", source.c_str(),
                                  static_cast<asUINT>(source.size()));
    if (r < 0) {
        impl_->setError("AngelScriptEngine: AddScriptSection failed: " +
                        std::to_string(r));
        return false;
    }

    r = mod->Build();
    if (r < 0) {
        impl_->setError("AngelScriptEngine: Build failed for module " +
                        moduleName + " (code " + std::to_string(r) + ")");
        return false;
    }
    return true;
#else
    (void)moduleName;
    (void)source;
    impl_->setError("AngelScriptEngine: built without ARTIFACT_HAS_ANGELSCRIPT (stub)");
    return false;
#endif
}

void AngelScriptEngine::discardModule(const std::string& moduleName) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
#ifdef ARTIFACT_HAS_ANGELSCRIPT
    if (impl_->engine_) {
        impl_->engine_->DiscardModule(moduleName.c_str());
    }
#else
    (void)moduleName;
#endif
}

bool AngelScriptEngine::runMain(const std::string& moduleName) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->lastError_.clear();

#ifdef ARTIFACT_HAS_ANGELSCRIPT
    if (!impl_->engine_) {
        impl_->setError("AngelScriptEngine: engine not initialized");
        return false;
    }

    asIScriptModule* mod = impl_->engine_->GetModule(moduleName.c_str());
    if (!mod) {
        impl_->setError("AngelScriptEngine: module not found: " + moduleName);
        return false;
    }

    asIScriptFunction* fn = mod->GetFunctionByDecl("void main()");
    if (!fn) {
        impl_->setError("AngelScriptEngine: 'void main()' not found in " + moduleName);
        return false;
    }

    asIScriptContext* ctx = impl_->engine_->CreateContext();
    if (!ctx) {
        impl_->setError("AngelScriptEngine: CreateContext failed");
        return false;
    }

    int r = ctx->Prepare(fn);
    if (r < 0) {
        impl_->setError("AngelScriptEngine: Prepare failed: " + std::to_string(r));
        ctx->Release();
        return false;
    }

    r = ctx->Execute();
    if (r != asEXECUTION_FINISHED) {
        impl_->setError("AngelScriptEngine: Execute did not finish cleanly (code " +
                        std::to_string(r) + ")");
        ctx->Release();
        return false;
    }

    ctx->Release();
    return true;
#else
    (void)moduleName;
    impl_->setError("AngelScriptEngine: built without ARTIFACT_HAS_ANGELSCRIPT (stub)");
    return false;
#endif
}

// ============================================================================
// Error Handling
// ============================================================================

std::string AngelScriptEngine::getLastError() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->lastError_;
}

bool AngelScriptEngine::hasError() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return !impl_->lastError_.empty();
}

void AngelScriptEngine::clearError() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->lastError_.clear();
}

} // namespace ArtifactCore
