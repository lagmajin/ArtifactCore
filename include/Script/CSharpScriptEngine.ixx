module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

export module Script.CSharp.Engine;

export namespace ArtifactCore {

struct ScriptSessionSnapshot {
    bool active = false;
    bool stopRequested = false;
    std::string sourcePath;
    double timeSeconds = 0.0;
    double deltaSeconds = 0.0;
    std::uint64_t frame = 0;
    std::string lastError;
};

/**
 * @brief Embedded .NET/C# scripting engine for Artifact.
 *
 * Provides Nuke/Maya/Houdini-style scripting integration via .NET hostfxr.
 * Loads and executes C# assemblies (.dll) through the .NET runtime.
 *
 * Usage:
 *   auto& cs = CSharpScriptEngine::instance();
 *   cs.initialize("C:/Program Files/dotnet");
 *   cs.execute("path/to/Script.dll");
 *   std::string result = cs.evaluate("MyNamespace.MyClass", "MyMethod");
 *   cs.finalize();
 */
class LIBRARY_DLL_API CSharpScriptEngine {
private:
    class Impl;
    Impl* impl_;

    CSharpScriptEngine();
    ~CSharpScriptEngine();
    CSharpScriptEngine(const CSharpScriptEngine&) = delete;
    CSharpScriptEngine& operator=(const CSharpScriptEngine&) = delete;

public:
    static CSharpScriptEngine& instance();

    // === Lifecycle ===
    bool initialize(const std::string& dotnetRoot = "");
    void setScriptHostAssemblyPath(const std::string& path);
    void finalize();
    bool isInitialized() const;

    // === Script Execution ===

    /// Load a C# assembly and execute its entry point (typically DllMain or a startup method).
    bool execute(const std::string& assemblyPath);

    /// Load a C# assembly. Returns true on success.
    bool loadAssembly(const std::string& assemblyPath);

    /// Evaluate a function in a loaded assembly. Returns the result as string.
    std::string evaluate(const std::string& typeName, const std::string& methodName, const std::string& argument = "");

    /// Evaluate C# code through the bundled Roslyn scripting host.
    bool executeScript(const std::string& code);
    bool executeScriptFile(const std::string& path);
    bool executeScriptWithImports(const std::string& code,
                                   const std::vector<std::string>& imports);

    // === Iterative script session ===
    // Keeps Roslyn ScriptState between evaluations, similar to an editor
    // play session. A failed step does not replace the last successful state.
    bool beginScriptSession(const std::string& code = "");
    bool beginScriptSessionFile(const std::string& path);
    bool stepScriptSession(const std::string& code);
    bool updateScriptSession(const std::string& code,
                             double timeSeconds,
                             double deltaSeconds,
                             std::uint64_t frame);
    /// File-driven tick; a changed source is reloaded once and is itself the
    /// evaluation for that tick, avoiding duplicate side effects.
    bool updateScriptSessionFile(const std::string& path,
                                 double timeSeconds,
                                 double deltaSeconds,
                                 std::uint64_t frame);
    /// Update session globals and evaluate the conventional C# Update() body.
    bool tickScriptSession(double timeSeconds,
                           double deltaSeconds,
                           std::uint64_t frame);
    bool invokeScriptSessionCallback(const std::string& functionName);
    bool reloadScriptSession(const std::string& code);
    bool reloadScriptSessionFile(const std::string& path);
    bool isScriptSessionSourceChanged() const;
    void endScriptSession();
    bool isScriptSessionActive() const;
    void requestScriptSessionStop();
    void clearScriptSessionStopRequest();
    bool isScriptSessionStopRequested() const;
    ScriptSessionSnapshot scriptSessionSnapshot() const;

    // === Output Capture ===

    using OutputCallback = std::function<void(const std::string& text, bool isError)>;
    void setOutputCallback(OutputCallback callback);

    // === Error Handling ===
    std::string getLastError() const;
    bool hasError() const;
    void clearError();
};

} // namespace ArtifactCore
