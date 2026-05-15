module;
#include <utility>
// pybind11-based embedded Python interpreter for Artifact
// pybind11 wraps CPython C API with clean C++ semantics.
// If pybind11 is not available, compiles as a no-op stub.
#ifdef ARTIFACT_HAS_PYTHON
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
namespace py = pybind11;
#endif

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>

module Script.Python.Engine;

namespace ArtifactCore {

// ============================================================================
// Implementation (pybind11-based)
// ============================================================================

class PythonEngine::Impl {
public:
    bool initialized_ = false;
    std::string lastError_;
    OutputCallback outputCallback_;
    mutable std::mutex mutex_;

    // Registered C++ functions (callable from Python)
    std::unordered_map<std::string, PyCppFunction> registeredFunctions_;

    // Interactive console state
    std::string consoleBuffer_;

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

    void setError(const std::string& err) {
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
        impl_->setError(std::string("Python init error: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        impl_->setError(std::string("Python init error: ") + e.what());
        return false;
    }

#else
    impl_->setError("Python support not compiled (ARTIFACT_HAS_PYTHON not defined). "
                     "Install pybind11 and define ARTIFACT_HAS_PYTHON to enable.");
    return false;
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
    impl_->setError("Python not available");
    return false;
#endif
}

bool PythonEngine::executeFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        impl_->setError("Cannot open file: " + filePath);
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return execute(ss.str());
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
    return "";
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
#endif
}

void PythonEngine::registerConstantInt(const std::string& name, int64_t value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        impl_->artifactModule_.attr(name.c_str()) = value;
    }
#endif
}

void PythonEngine::registerConstantFloat(const std::string& name, double value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (impl_->initialized_) {
        impl_->artifactModule_.attr(name.c_str()) = value;
    }
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
#endif
}

void PythonEngine::setGlobalInt(const std::string& name, int64_t value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#endif
}

void PythonEngine::setGlobalFloat(const std::string& name, double value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
#endif
}

void PythonEngine::setGlobalBool(const std::string& name, bool value) {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->globals_[name.c_str()] = value;
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
    return "";
}

int64_t PythonEngine::getGlobalInt(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return 0;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<int64_t>();
    } catch (...) {}
#endif
    return 0;
}

double PythonEngine::getGlobalFloat(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return 0.0;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<double>();
    } catch (...) {}
#endif
    return 0.0;
}

bool PythonEngine::getGlobalBool(const std::string& name) const {
#ifdef ARTIFACT_HAS_PYTHON
    if (!impl_->initialized_) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        return impl_->globals_[name.c_str()].cast<bool>();
    } catch (...) {}
#endif
    return false;
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

std::string PythonEngine::getLastError() const { return impl_->lastError_; }
bool PythonEngine::hasError() const { return !impl_->lastError_.empty(); }
void PythonEngine::clearError() { impl_->lastError_.clear(); }

// ============================================================================
// Interactive Console
// ============================================================================

bool PythonEngine::pushConsoleLine(const std::string& line) {
    if (!impl_->initialized_) return false;

    impl_->consoleBuffer_ += line + "\n";

#ifdef ARTIFACT_HAS_PYTHON
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    try {
        // Try to compile the accumulated buffer
        auto code = py::module_::import("code");
        auto compileResult = code.attr("compile_command")(impl_->consoleBuffer_, "<console>", "exec");

        if (compileResult.is_none()) {
            // Incomplete input - need more
            return true;
        }

        // Complete statement - execute
        py::exec(impl_->consoleBuffer_, impl_->globals_);

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
    impl_->consoleBuffer_.clear();
    return false;
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
#endif
    return paths;
}

} // namespace ArtifactCore
