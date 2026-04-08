module;
#include "../../Define/DllExportMacro.hpp"
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Script.Python.Engine;





export namespace ArtifactCore {

/**
 * @brief Embedded Python Interpreter for Artifact.
 * Provides Nuke/Maya/Houdini-style Python scripting integration.
 *
 * Features:
 *  - Initialize/finalize CPython interpreter
 *  - Execute Python scripts (string or file)
 *  - Register C++ functions/objects into the 'artifact' Python module
 *  - Bidirectional data exchange (C++ <-> Python)
 *  - Thread-safe GIL management
 *  - Output capture (stdout/stderr -> application console)
 *
 * Usage:
 *   auto& py = PythonEngine::instance();
 *   py.initialize();
 *   py.execute("import artifact; print(artifact.version())");
 *   py.executeFile("scripts/setup_scene.py");
 *   py.finalize();
 */
class LIBRARY_DLL_API PythonEngine {
private:
    class Impl;
    Impl* impl_;

    PythonEngine();
    ~PythonEngine();
    PythonEngine(const PythonEngine&) = delete;
    PythonEngine& operator=(const PythonEngine&) = delete;

public:
    static PythonEngine& instance();

    // === Lifecycle ===
    bool initialize(const std::string& pythonHome = "");
    void finalize();
    bool isInitialized() const;

    // === Script Execution ===
    
    /// Execute a Python code string. Returns true on success.
    bool execute(const std::string& code);

    /// Execute a Python file. Returns true on success.
    bool executeFile(const std::string& filePath);

    /// Evaluate a Python expression and return the result as string.
    std::string evaluate(const std::string& expression);

    // === Module Registration ===

    /// Callback type for registering C++ functions into Python
    using PyCppFunction = std::function<std::string(const std::vector<std::string>&)>;

    /// Register a C++ function callable from Python as artifact.<name>(args...)
    void registerFunction(const std::string& name, PyCppFunction func);

    /// Register a constant value accessible as artifact.<name>
    void registerConstant(const std::string& name, const std::string& value);
    void registerConstantInt(const std::string& name, int64_t value);
    void registerConstantFloat(const std::string& name, double value);

    // === Variable Exchange ===

    /// Set a Python global variable
    void setGlobalString(const std::string& name, const std::string& value);
    void setGlobalInt(const std::string& name, int64_t value);
    void setGlobalFloat(const std::string& name, double value);
    void setGlobalBool(const std::string& name, bool value);

    /// Get a Python global variable
    std::string getGlobalString(const std::string& name) const;
    int64_t getGlobalInt(const std::string& name) const;
    double getGlobalFloat(const std::string& name) const;
    bool getGlobalBool(const std::string& name) const;

    // === Output Capture ===

    /// Callback for captured stdout/stderr output
    using OutputCallback = std::function<void(const std::string& text, bool isError)>;
    void setOutputCallback(OutputCallback callback);

    // === Error Handling ===
    std::string getLastError() const;
    bool hasError() const;
    void clearError();

    // === Interactive Console ===
    
    /// Push a line of code to the interactive console (supports multi-line input).
    /// Returns true if more input is needed (incomplete statement).
    bool pushConsoleLine(const std::string& line);

    /// Reset the interactive console state.
    void resetConsole();

    // === Path Management ===

    /// Add a directory to Python's sys.path
    void addSearchPath(const std::string& path);

    /// Get current sys.path
    std::vector<std::string> getSearchPaths() const;
};

} // namespace ArtifactCore
