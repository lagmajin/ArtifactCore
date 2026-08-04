module;
#include <utility>
// pybind11-based embedded Python interpreter for Artifact
// pybind11 wraps CPython C API with clean C++ semantics.
// If pybind11 is not available, the implementation falls back to an external
// Python process so the public engine contract remains usable in minimal builds.
#ifdef ARTIFACT_HAS_PYTHON
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#endif

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <iterator>
#include <string_view>
#include <iostream>
#include <QProcess>
#include <QString>

module Script.Python.Engine;

import Core.ArtifactString;

#ifdef ARTIFACT_HAS_PYTHON
namespace py = pybind11;
#endif

namespace ArtifactCore {

// ============================================================================
// Implementation (pybind11-based)
// ============================================================================

class PythonEngine::Impl {
public:
    bool initialized_ = false;
    ZeroString lastError_;
    OutputCallback outputCallback_;
    mutable std::mutex mutex_;

    // Registered C++ functions (callable from Python)
    std::unordered_map<std::string, PyCppFunction> registeredFunctions_;

    // Interactive console state
    ZeroString consoleBuffer_;
    bool externalRuntime_ = false;
    std::string externalExecutable_ = "python";
    std::vector<std::string> externalSearchPaths_;
    std::string externalPrelude_;
    std::unordered_map<std::string, std::string> externalStringGlobals_;
    std::unordered_map<std::string, int64_t> externalIntGlobals_;
    std::unordered_map<std::string, double> externalFloatGlobals_;
    std::unordered_map<std::string, bool> externalBoolGlobals_;

    void setExternalValue(const std::string& name, const std::string& literal) {
        if (!name.empty()) externalPrelude_ += name + " = " + literal + "\n";
    }

#ifdef ARTIFACT_HAS_PYTHON
    std::unique_ptr<py::scoped_interpreter> guard_;
    py::module_ artifactModule_;
    py::dict globals_;
#endif

    Impl() {}
    ~Impl() {}

    void captureOutput(const std::string& text, bool isError) {
        if (outputCallback_) {
            outputCallback_(text, isError);
        }
    }

    void captureOutput(const ZeroString& text, bool isError) {
        if (outputCallback_) {
            outputCallback_(std::string(text), isError);
        }
    }

    void setError(const char* err) {
        lastError_ = ZeroString(err);
        captureOutput(lastError_, true);
    }

    void setError(std::string_view err) {
        lastError_ = err;
        captureOutput(lastError_, true);
    }

    void setError(const ZeroString& err) {
        lastError_ = err;
        captureOutput(err, true);
    }
};

// ============================================================================
// Singleton
// ============================================================================

PythonEngine::PythonEngine() : impl_(new Impl()) {}

PythonEngine::~PythonEngine() {
    finalize();
    delete impl_;
}

PythonEngine& PythonEngine::instance() {
    static PythonEngine instance;
    return instance;
}

// ============================================================================
// Lifecycle
// ============================================================================

bool PythonEngine::initialize(const std::string& pythonHome) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->initialized_) return true;

#ifdef ARTIFACT_HAS_PYTHON
    try {
        // Start embedded interpreter via pybind11 RAII guard
        impl_->guard_ = std::make_unique<py::scoped_interpreter>();

        // Grab __main__ globals
        auto mainModule = py::module_::import("__main__");
        impl_->globals_ = mainModule.attr("__dict__").cast<py::dict>();

        // Create the 'artifact' module (Nuke/Maya style)
        impl_->artifactModule_ = py::module_::import("types").attr("ModuleType")("artifact");
        impl_->artifactModule_.attr("__doc__") = "Artifact Python API - Nuke/Maya-style scripting";
        impl_->artifactModule_.attr("_version") = "1.0.0";
        impl_->artifactModule_.attr("_app_name") = "Artifact";

        // Version function
        impl_->artifactModule_.attr("version") = py::cpp_function([]() {
            return std::string("1.0.0");
        });

        // Register the module into sys.modules
        py::module_::import("sys").attr("modules")["artifact"] = impl_->artifactModule_;

        // Setup stdout/stderr capture
        py::exec(R"(
import sys, io
class _ArtifactOut:
    def __init__(self, err=False):
        self.err = err
        self._buf = io.StringIO()
    def write(self, s):
        self._buf.write(s)
    def flush(self):
        pass
    def getvalue(self):
        v = self._buf.getvalue()
        self._buf = io.StringIO()
        return v
sys.stdout = _ArtifactOut(False)
sys.stderr = _ArtifactOut(True)
)");

        impl_->initialized_ = true;

        // Inject any pre-registered C++ functions
        for (const auto& [name, func] : impl_->registeredFunctions_) {
            auto wrappedFunc = [func](py::args args) -> std::string {
                std::vector<std::string> strArgs;
                for (auto& a : args) {
                    strArgs.push_back(py::str(a).cast<std::string>());
                }
                return func(strArgs);
            };
            impl_->artifactModule_.attr(name.c_str()) = py::cpp_function(wrappedFunc);
        }

        return true;

    } catch (const py::error_already_set& e) {
        impl_->setError(ZeroString("Python init error: ") + std::string_view(e.what()));
        return false;
    } catch (const std::exception& e) {
        impl_->setError(ZeroString("Python init error: ") + std::string_view(e.what()));
        return false;
    }

#else
    std::string executablePath = pythonHome;
    if (!executablePath.empty() && std::filesystem::is_directory(executablePath)) {
#ifdef _WIN32
        executablePath += "/python.exe";
#else
        executablePath += "/bin/python3";
#endif
    }
    const QString executable = executablePath.empty()
        ? QStringLiteral("python") : QString::fromStdString(executablePath);
    QProcess probe;
    probe.setProgram(executable);
    probe.setArguments({QStringLiteral("--version")});
    probe.start();
    if (!probe.waitForStarted(2000) || !probe.waitForFinished(5000) ||
        probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        impl_->setError("Python support unavailable: embedded pybind11 is not enabled and external Python was not found.");
        return false;
    }
    impl_->externalExecutable_ = executable.toStdString();
    impl_->externalRuntime_ = true;
    impl_->initialized_ = true;
    impl_->lastError_.clear();
    return true;
#endif
}

void PythonEngine::finalize() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!impl_->initialized_) return;

#ifdef ARTIFACT_HAS_PYTHON
    impl_->artifactModule_ = py::module_();
    impl_->globals_ = py::dict();
    impl_->guard_.reset(); // Destroy interpreter
#endif

    impl_->initialized_ = false;
    impl_->externalRuntime_ = false;
}

bool PythonEngine::isInitialized() const {
    return impl_->initialized_;
}

// ============================================================================
// Script Execution
// ============================================================================

bool PythonEngine::execute(const std::string& code) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!impl_->initialized_) {
        impl_->setError("Python not initialized");
        return false;
    }

#ifdef ARTIFACT_HAS_PYTHON
    try {
        py::exec(code, impl_->globals_);

        // Flush captured output
        auto sys = py::module_::import("sys");
        auto stdoutVal = sys.attr("stdout").attr("getvalue")().cast<std::string>();
        if (!stdoutVal.empty()) impl_->captureOutput(stdoutVal, false);

        auto stderrVal = sys.attr("stderr").attr("getvalue")().cast<std::string>();
        if (!stderrVal.empty()) impl_->captureOutput(stderrVal, true);

        impl_->lastError_.clear();
        return true;

    } catch (const py::error_already_set& e) {
        impl_->setError(e.what());
        return false;
    }
#else
    if (!impl_->externalRuntime_) {
        impl_->setError("Python not initialized");
        return false;
    }
    QProcess process;
    process.setProgram(QString::fromStdString(impl_->externalExecutable_));
    std::string externalCode;
    for (const auto& path : impl_->externalSearchPaths_) {
        std::string escaped = path;
        size_t pos = 0;
        while ((pos = escaped.find('\\', pos)) != std::string::npos) {
            escaped.insert(pos, "\\");
            pos += 2;
        }
        pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.insert(pos, "\\");
            pos += 2;
        }
        externalCode += "import sys; sys.path.insert(0, \"" + escaped + "\");\n";
    }
    externalCode += impl_->externalPrelude_;
    externalCode += code;
    process.setArguments({QStringLiteral("-c"), QString::fromStdString(externalCode)});
    process.start();
    if (!process.waitForStarted(2000) || !process.waitForFinished(30000)) {
        impl_->setError("External Python process did not finish");
        process.kill();
        return false;
    }
    const QByteArray stdoutData = process.readAllStandardOutput();
    const QByteArray stderrData = process.readAllStandardError();
    if (!stdoutData.isEmpty()) impl_->captureOutput(stdoutData.toStdString(), false);
    if (!stderrData.isEmpty()) impl_->captureOutput(stderrData.toStdString(), true);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        impl_->setError(stderrData.isEmpty() ? "External Python execution failed"
                                             : stderrData.toStdString());
        return false;
    }
    impl_->lastError_.clear();
    return true;
#endif
}

bool PythonEngine::executeFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        ZeroString error = "Cannot open file: ";
        error += filePath;
        impl_->setError(error);
        return false;
    }
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return execute(code);
}

std::string PythonEngine::evaluate(const std::string& expression) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!impl_->initialized_) return "";

#ifdef ARTIFACT_HAS_PYTHON
    try {
        py::object result = py::eval(expression, impl_->globals_);
        return py::str(result).cast<std::string>();
    } catch (const py::error_already_set&) {
        return "";
    }
#else
    if (!impl_->externalRuntime_) return "";
    QProcess process;
    process.setProgram(QString::fromStdString(impl_->externalExecutable_));
    const std::string script = impl_->externalPrelude_ +
        "print(repr(" + expression + "))";
    process.setArguments({QStringLiteral("-c"), QString::fromStdString(script)});
    process.start();
    if (!process.waitForStarted(2000) || !process.waitForFinished(30000) ||
        process.exitCode() != 0) return "";
    QByteArray output = process.readAllStandardOutput().trimmed();
    return output.toStdString();
#endif
}

// ============================================================================
// Module Registration
// ============================================================================

void PythonEngine::registerFunction(const std::string& name, PyCppFunction func) {
    impl_->registeredFunctions_[name] = func;

#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        auto wrappedFunc = [func](py::args args) -> std::string {
            std::vector<std::string> strArgs;
            for (auto& a : args) {
                strArgs.push_back(py::str(a).cast<std::string>());
            }
            return func(strArgs);
        };
        impl_->artifactModule_.attr(name.c_str()) = py::cpp_function(wrappedFunc);
    }
#endif
}

void PythonEngine::registerConstant(const std::string& name, const std::string& value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        impl_->artifactModule_.attr(name.c_str()) = value;
    }
#else
    std::string escaped = value;
    size_t pos = 0;
    while ((pos = escaped.find('\\', pos)) != std::string::npos) { escaped.insert(pos, "\\"); pos += 2; }
    pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) { escaped.insert(pos, "\\"); pos += 2; }
    impl_->setExternalValue(name, "\"" + escaped + "\"");
#endif
}

void PythonEngine::registerConstantInt(const std::string& name, int64_t value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        impl_->artifactModule_.attr(name.c_str()) = value;
    }
#else
    impl_->setExternalValue(name, std::to_string(value));
#endif
}

void PythonEngine::registerConstantFloat(const std::string& name, double value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        impl_->artifactModule_.attr(name.c_str()) = value;
    }
#else
    impl_->setExternalValue(name, std::to_string(value));
#endif
}

// ============================================================================
// Variable Exchange (pybind11 makes this very clean)
// ============================================================================

void PythonEngine::setGlobalString(const std::string& name, const std::string& value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#else
    registerConstant(name, value);
    impl_->externalStringGlobals_[name] = value;
#endif
}

void PythonEngine::setGlobalInt(const std::string& name, int64_t value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#else
    impl_->setExternalValue(name, std::to_string(value));
    impl_->externalIntGlobals_[name] = value;
#endif
}

void PythonEngine::setGlobalFloat(const std::string& name, double value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#else
    impl_->setExternalValue(name, std::to_string(value));
    impl_->externalFloatGlobals_[name] = value;
#endif
}

void PythonEngine::setGlobalBool(const std::string& name, bool value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#else
    impl_->setExternalValue(name, value ? "True" : "False");
    impl_->externalBoolGlobals_[name] = value;
#endif
}

std::string PythonEngine::getGlobalString(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return "";
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<std::string>();
    } catch (...) {}
#endif
    auto it = impl_->externalStringGlobals_.find(name);
    return it == impl_->externalStringGlobals_.end() ? std::string() : it->second;
}

int64_t PythonEngine::getGlobalInt(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return 0;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<int64_t>();
    } catch (...) {}
#endif
    auto it = impl_->externalIntGlobals_.find(name);
    return it == impl_->externalIntGlobals_.end() ? 0 : it->second;
}

double PythonEngine::getGlobalFloat(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return 0.0;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<double>();
    } catch (...) {}
#endif
    auto it = impl_->externalFloatGlobals_.find(name);
    return it == impl_->externalFloatGlobals_.end() ? 0.0 : it->second;
}

bool PythonEngine::getGlobalBool(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<bool>();
    } catch (...) {}
#endif
    auto it = impl_->externalBoolGlobals_.find(name);
    return it == impl_->externalBoolGlobals_.end() ? false : it->second;
}

// ============================================================================
// Output Capture
// ============================================================================

void PythonEngine::setOutputCallback(OutputCallback callback) {
    impl_->outputCallback_ = callback;
}

// ============================================================================
// Error Handling
// ============================================================================

std::string PythonEngine::getLastError() const {
    return std::string(impl_->lastError_.data(), impl_->lastError_.length());
}
bool PythonEngine::hasError() const { return impl_->lastError_.length() != 0; }
void PythonEngine::clearError() { impl_->lastError_.clear(); }

// ============================================================================
// Interactive Console
// ============================================================================

bool PythonEngine::pushConsoleLine(const std::string& line) {
    if (!impl_->initialized_) return false;

    impl_->consoleBuffer_ += line;
    impl_->consoleBuffer_ += '\n';

#ifdef ARTIFACT_HAS_PYTHON
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        // Try to compile the accumulated buffer
        auto code = py::module_::import("code");
        std::string_view consoleText = impl_->consoleBuffer_;
        auto compileResult = code.attr("compile_command")(consoleText, "<console>", "exec");

        if (compileResult.is_none()) {
            // Incomplete input - need more
            return true;
        }

        // Complete statement - execute
        py::exec(consoleText, impl_->globals_);

        // Flush output
        auto sys = py::module_::import("sys");
        auto out = sys.attr("stdout").attr("getvalue")().cast<std::string>();
        if (!out.empty()) impl_->captureOutput(out, false);

        impl_->consoleBuffer_.clear();
        return false;

    } catch (const py::error_already_set& e) {
        impl_->setError(e.what());
        impl_->consoleBuffer_.clear();
        return false;
    }
#else
    const std::string buffered = std::string(impl_->consoleBuffer_.data(),
                                             impl_->consoleBuffer_.length());
    int parenDepth = 0;
    bool inSingle = false;
    bool inDouble = false;
    bool escaped = false;
    for (const char ch : buffered) {
        if (escaped) { escaped = false; continue; }
        if ((inSingle || inDouble) && ch == '\\') { escaped = true; continue; }
        if (!inDouble && ch == '\'') { inSingle = !inSingle; continue; }
        if (!inSingle && ch == '"') { inDouble = !inDouble; continue; }
        if (inSingle || inDouble) continue;
        if (ch == '(' || ch == '[' || ch == '{') ++parenDepth;
        if (ch == ')' || ch == ']' || ch == '}') parenDepth = std::max(0, parenDepth - 1);
    }
    const std::string trimmed = buffered.substr(buffered.find_last_not_of(" \t\r\n") + 1);
    const bool blockContinues = !trimmed.empty() && trimmed.back() == ':';
    const bool lineContinues = !trimmed.empty() && trimmed.back() == '\\';
    if (parenDepth > 0 || inSingle || inDouble || blockContinues || lineContinues) return true;
    impl_->consoleBuffer_.clear();
    return !execute(buffered);
#endif
}

void PythonEngine::resetConsole() {
    impl_->consoleBuffer_.clear();
}

// ============================================================================
// Path Management
// ============================================================================

void PythonEngine::addSearchPath(const std::string& path) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto sys = py::module_::import("sys");
    sys.attr("path").attr("insert")(0, path);
#else
    if (!path.empty() && std::find(impl_->externalSearchPaths_.begin(),
                                   impl_->externalSearchPaths_.end(), path) ==
                            impl_->externalSearchPaths_.end()) {
        impl_->externalSearchPaths_.push_back(path);
    }
#endif
}

std::vector<std::string> PythonEngine::getSearchPaths() const {
    std::vector<std::string> paths;
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return paths;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        auto sys = py::module_::import("sys");
        auto pyPath = sys.attr("path").cast<py::list>();
        for (auto& item : pyPath) {
            paths.push_back(item.cast<std::string>());
        }
    } catch (...) {}
#else
    paths = impl_->externalSearchPaths_;
#endif
    return paths;
}

} // namespace ArtifactCore
