module;
#include <utility>

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

// Immutable view of the host that expressions and scripts are evaluated
// against. Everything the expression language exposes as thisComp / thisLayer /
// time comes from here, so a snapshot must describe the real composition rather
// than a placeholder. Composition dimensions, timing and the layer catalog used
// to be absent, which left thisComp.width/height hardcoded and
// thisComp.layer() searching only the current selection.
struct ScriptHostSnapshot {
    std::string appName = "Artifact";
    std::string appVersion = "1.0.0";
    std::string projectName;
    std::string activeCompositionName;
    std::string workingDirectory;
    // Layer names in the active composition, ordered bottom-to-top. This is the
    // catalog thisComp.layer() resolves against, and it is deliberately
    // independent of `selection`.
    std::vector<std::string> layerNames;
    std::vector<std::string> selection;
    bool hasProject = false;
    bool hasComposition = false;
    // Composition geometry. Zero means "unknown" and is published as 0 so
    // expressions can detect the absence instead of reading a fake HD value.
    int compositionWidth = 0;
    int compositionHeight = 0;
    double frameRate = 0.0;
    // Playback position and length, both in seconds. Zero is a valid value, so
    // hasTiming distinguishes "no composition" from "playing at time 0".
    double timeSeconds = 0.0;
    double durationSeconds = 0.0;
    bool hasTiming = false;
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

    // Composition state. Kept as separate setters so a host can push only what
    // changed on a layer/composition/playback event instead of rebuilding a
    // whole snapshot every time.
    void setCompositionGeometry(int width, int height);
    void setLayerNames(const std::vector<std::string>& layerNames);
    void setTiming(double timeSeconds, double durationSeconds, double frameRate);

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
