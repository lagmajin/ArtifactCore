module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

export module Script.CSharp.Engine;

export namespace ArtifactCore {

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
    void finalize();
    bool isInitialized() const;

    // === Script Execution ===

    /// Load a C# assembly and execute its entry point (typically DllMain or a startup method).
    bool execute(const std::string& assemblyPath);

    /// Load a C# assembly. Returns true on success.
    bool loadAssembly(const std::string& assemblyPath);

    /// Evaluate a function in a loaded assembly. Returns the result as string.
    std::string evaluate(const std::string& typeName, const std::string& methodName, const std::string& argument = "");

    // === Output Capture ===

    using OutputCallback = std::function<void(const std::string& text, bool isError)>;
    void setOutputCallback(OutputCallback callback);

    // === Error Handling ===
    std::string getLastError() const;
    bool hasError() const;
    void clearError();
};

} // namespace ArtifactCore
