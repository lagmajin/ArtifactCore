module;

#include "../Define/DllExportMacro.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

export module Script.Runtime;

import Script.Expression.Evaluator;
import Script.Expression.Parser;
import Script.Expression.Value;

export namespace ArtifactCore {

enum class ScriptLogLevel {
    Info,
    Warning,
    Error
};

struct ScriptHostSnapshot {
    std::string appName = "Artifact";
    std::string appVersion = "1.0.0";
    std::string projectName;
    std::string activeCompositionName;
    std::string workingDirectory;
    std::vector<std::string> selection;
    bool hasProject = false;
    bool hasComposition = false;
};

struct ScriptExecutionResult {
    bool success = false;
    std::string output;
    std::string error;
    std::size_t errorPosition = std::string::npos;
    std::size_t errorLength = 0;
    std::size_t errorLine = 0;
    std::size_t errorColumn = 0;
};

class LIBRARY_DLL_API ScriptRuntime {
private:
    class Impl;
    Impl* impl_;

public:
    ScriptRuntime();
    ~ScriptRuntime();
    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    using LogCallback = std::function<void(const std::string& message, ScriptLogLevel level)>;

    void setHostSnapshot(const ScriptHostSnapshot& snapshot);
    ScriptHostSnapshot hostSnapshot() const;

    void setAppName(const std::string& appName);
    void setAppVersion(const std::string& appVersion);
    void setProjectName(const std::string& projectName);
    void setActiveCompositionName(const std::string& compositionName);
    void setWorkingDirectory(const std::string& workingDirectory);
    void setSelection(const std::vector<std::string>& selection);

    void clearSelection();
    void setHasProject(bool hasProject);
    void setHasComposition(bool hasComposition);

    void setLanguageStyle(ExpressionLanguageStyle style);
    ExpressionLanguageStyle languageStyle() const;

    void setLogger(LogCallback callback);
    void clearLogger();

    ScriptExecutionResult execute(const std::string& source);
    ScriptExecutionResult executeFile(const std::filesystem::path& path);

    ScriptExecutionResult lastResult() const;
    std::string lastError() const;
    bool hasError() const;
    void clearError();

    void reset();
};

} // namespace ArtifactCore
