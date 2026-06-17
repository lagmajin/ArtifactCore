module;
#include "../../Define/DllExportMacro.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
export module Script.AngelScript.Engine;

export namespace ArtifactCore {

/**
 * @brief Embedded AngelScript engine for Artifact.
 *
 * Provides a Unity-MonoBehaviour-style scripting layer: serializable AngelScript
 * classes can be attached to layers as frame-driven components and interact
 * with transforms / properties. This engine is intentionally lightweight and
 * is distinct from the heavier Python automation layer (Script.Python.Engine)
 * and the numeric expression evaluator (Script.Expression).
 *
 * Features:
 *  - Create / discard an asIScriptEngine instance (singleton, Pimpl)
 *  - Compile a script module from source
 *  - Execute a free function in a compiled module
 *  - Register host functions (log/print) before compilation
 *  - Capture script output via callback
 *  - Stubbable: when ARTIFACT_HAS_ANGELSCRIPT is undefined, compiles as a no-op
 *
 * Usage:
 *   auto& as = AngelScriptEngine::instance();
 *   as.initialize();
 *   as.setOutputCallback([](const std::string& s, bool err){ ... });
 *   if (as.compileModule("mymod", "void main(){ log(\"ok\"); }")) {
 *       as.runMain("mymod");
 *   }
 */
class LIBRARY_DLL_API AngelScriptEngine {
private:
    class Impl;
    Impl* impl_;

    AngelScriptEngine();
    ~AngelScriptEngine();
    AngelScriptEngine(const AngelScriptEngine&) = delete;
    AngelScriptEngine& operator=(const AngelScriptEngine&) = delete;

public:
    static AngelScriptEngine& instance();

    // === Lifecycle ===

    /// Create the underlying asIScriptEngine and register core host functions.
    /// Safe to call multiple times; no-op if already initialized.
    bool initialize();

    /// Release the underlying engine and all compiled modules.
    void finalize();

    bool isInitialized() const;

    // === Output Capture ===

    using OutputCallback = std::function<void(const std::string& text, bool isError)>;
    void setOutputCallback(OutputCallback callback);

    // === Compilation & Execution ===

    /// Compile `source` into a module named `moduleName`.
    /// On success the module is retained until discardModule() or finalize().
    /// Returns false and records the last error on failure.
    bool compileModule(const std::string& moduleName, const std::string& source);

    /// Release a previously compiled module by name.
    void discardModule(const std::string& moduleName);

    /// Execute a free function by declaration in `moduleName`.
    /// Example: `void main()` or `void Update(float dt)`.
    bool runFunction(const std::string& moduleName, const std::string& functionDecl);

    /// Execute the free function `void main()` in `moduleName`.
    /// Returns false if the module or function is missing, or on runtime error.
    bool runMain(const std::string& moduleName);

    // === Error Handling ===
    std::string getLastError() const;
    bool hasError() const;
    void clearError();
};

} // namespace ArtifactCore
