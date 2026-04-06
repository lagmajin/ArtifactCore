module;

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>

module Script.Runtime;

namespace ArtifactCore {

namespace {

struct StatementSlice {
    std::string text;
    std::size_t offset = 0;
};

std::string trimCopy(const std::string& text)
{
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string stripTrailingComment(const std::string& text)
{
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escape = false;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];

        if (escape) {
            escape = false;
            continue;
        }

        if (ch == '\\') {
            escape = true;
            continue;
        }

        if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (ch == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote) {
            if (ch == '#') {
                return trimCopy(text.substr(0, i));
            }
            if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
                return trimCopy(text.substr(0, i));
            }
        }
    }

    return trimCopy(text);
}

std::vector<StatementSlice> splitStatements(const std::string& source)
{
    std::vector<StatementSlice> statements;
    std::string current;
    std::size_t currentStart = std::string::npos;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escape = false;

    auto flush = [&](std::size_t nextOffset) {
        const std::string trimmed = stripTrailingComment(current);
        if (!trimmed.empty()) {
            std::size_t offset = currentStart;
            if (offset == std::string::npos) {
                offset = nextOffset >= current.size() ? nextOffset - current.size() : 0;
            }
            const std::size_t trimmedBegin = current.find(trimmed);
            if (trimmedBegin != std::string::npos) {
                offset += trimmedBegin;
            }
            statements.push_back({trimmed, offset});
        }
        current.clear();
        currentStart = std::string::npos;
    };

    for (std::size_t i = 0; i < source.size(); ++i) {
        const char ch = source[i];

        if (currentStart == std::string::npos && !std::isspace(static_cast<unsigned char>(ch))) {
            currentStart = i;
        }

        if (escape) {
            current.push_back(ch);
            escape = false;
            continue;
        }

        if (ch == '\\') {
            current.push_back(ch);
            escape = true;
            continue;
        }

        if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.push_back(ch);
            continue;
        }

        if (ch == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            current.push_back(ch);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && (ch == '\n' || ch == ';')) {
            flush(i + 1);
            continue;
        }

        current.push_back(ch);
    }

    flush(source.size());
    return statements;
}

std::pair<std::size_t, std::size_t> lineColumnForOffset(const std::string& source, std::size_t offset)
{
    std::size_t line = 1;
    std::size_t column = 1;
    const std::size_t boundedOffset = std::min(offset, source.size());
    for (std::size_t i = 0; i < boundedOffset; ++i) {
        if (source[i] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return {line, column};
}

std::string joinStrings(const std::vector<ExpressionValue>& values)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            stream << ", ";
        }
        stream << values[i].asString();
    }
    return stream.str();
}

} // namespace

class ScriptRuntime::Impl {
public:
    mutable std::mutex mutex_;
    ScriptHostSnapshot hostSnapshot_;
    ExpressionParser parser_;
    ExpressionEvaluator evaluator_;
    LogCallback logger_;
    ScriptExecutionResult lastResult_;
    ExpressionLanguageStyle languageStyle_ = ExpressionLanguageStyle::JavaScript;

    Impl()
    {
        evaluator_.registerFunction("log", [this](const std::vector<ExpressionValue>& args, const ExpressionEvaluator*) {
            const std::string message = joinStrings(args);
            if (logger_) {
                logger_(message, ScriptLogLevel::Info);
            }
            return ExpressionValue(message);
        });
        evaluator_.registerFunction("warn", [this](const std::vector<ExpressionValue>& args, const ExpressionEvaluator*) {
            const std::string message = joinStrings(args);
            if (logger_) {
                logger_(message, ScriptLogLevel::Warning);
            }
            return ExpressionValue(message);
        });
        evaluator_.registerFunction("error", [this](const std::vector<ExpressionValue>& args, const ExpressionEvaluator*) {
            const std::string message = joinStrings(args);
            if (logger_) {
                logger_(message, ScriptLogLevel::Error);
            }
            return ExpressionValue(message);
        });
        syncParserStyle();
    }

    void syncParserStyle()
    {
        parser_.setLanguageStyle(languageStyle_);
    }

    void updateHostVariables()
    {
        std::map<std::string, ExpressionValue> vars;
        vars["app_name"] = ExpressionValue(hostSnapshot_.appName);
        vars["app_version"] = ExpressionValue(hostSnapshot_.appVersion);
        vars["project_name"] = ExpressionValue(hostSnapshot_.projectName);
        vars["active_composition_name"] = ExpressionValue(hostSnapshot_.activeCompositionName);
        vars["working_directory"] = ExpressionValue(hostSnapshot_.workingDirectory);
        vars["has_project"] = ExpressionValue(hostSnapshot_.hasProject ? 1.0 : 0.0);
        vars["has_composition"] = ExpressionValue(hostSnapshot_.hasComposition ? 1.0 : 0.0);
        vars["selection_count"] = ExpressionValue(static_cast<double>(hostSnapshot_.selection.size()));
        std::vector<ExpressionValue> selectionValues;
        selectionValues.reserve(hostSnapshot_.selection.size());
        for (const auto& item : hostSnapshot_.selection) {
            selectionValues.emplace_back(item);
        }
        vars["selection"] = ExpressionValue(selectionValues);
        vars["selection_names"] = ExpressionValue(selectionValues);
        evaluator_.setVariables(vars);
    }

    void fail(ScriptExecutionResult& result,
              const std::string& message,
              std::size_t position = std::string::npos,
              std::size_t length = 0,
              const std::string& source = std::string())
    {
        result.success = false;
        result.error = message;
        result.errorPosition = position;
        result.errorLength = length;
        if (position != std::string::npos && !source.empty()) {
            const auto [line, column] = lineColumnForOffset(source, position);
            result.errorLine = line;
            result.errorColumn = column;
        }
        if (logger_) {
            logger_(message, ScriptLogLevel::Error);
        }
        lastResult_ = result;
    }
};

ScriptRuntime::ScriptRuntime() : impl_(new Impl())
{
}

ScriptRuntime::~ScriptRuntime()
{
    delete impl_;
}

void ScriptRuntime::setHostSnapshot(const ScriptHostSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_ = snapshot;
}

ScriptHostSnapshot ScriptRuntime::hostSnapshot() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->hostSnapshot_;
}

void ScriptRuntime::setAppName(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.appName = appName;
}

void ScriptRuntime::setAppVersion(const std::string& appVersion)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.appVersion = appVersion;
}

void ScriptRuntime::setProjectName(const std::string& projectName)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.projectName = projectName;
    impl_->hostSnapshot_.hasProject = !projectName.empty();
}

void ScriptRuntime::setActiveCompositionName(const std::string& compositionName)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.activeCompositionName = compositionName;
    impl_->hostSnapshot_.hasComposition = !compositionName.empty();
}

void ScriptRuntime::setWorkingDirectory(const std::string& workingDirectory)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.workingDirectory = workingDirectory;
}

void ScriptRuntime::setSelection(const std::vector<std::string>& selection)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.selection = selection;
}

void ScriptRuntime::clearSelection()
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.selection.clear();
}

void ScriptRuntime::setHasProject(bool hasProject)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.hasProject = hasProject;
}

void ScriptRuntime::setHasComposition(bool hasComposition)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_.hasComposition = hasComposition;
}

void ScriptRuntime::setLanguageStyle(ExpressionLanguageStyle style)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->languageStyle_ = style;
    impl_->syncParserStyle();
}

ExpressionLanguageStyle ScriptRuntime::languageStyle() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->languageStyle_;
}

void ScriptRuntime::setLogger(LogCallback callback)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->logger_ = std::move(callback);
}

void ScriptRuntime::clearLogger()
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->logger_ = nullptr;
}

ScriptExecutionResult ScriptRuntime::execute(const std::string& source)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    ScriptExecutionResult result;

    const std::string trimmed = trimCopy(source);
    if (trimmed.empty()) {
        impl_->fail(result, "Script source is empty", std::string::npos, 0, source);
        return result;
    }

    impl_->syncParserStyle();
    impl_->updateHostVariables();

    const auto statements = splitStatements(source);
    if (statements.empty()) {
        impl_->fail(result, "No executable statements found", std::string::npos, 0, source);
        return result;
    }

    std::ostringstream output;
    bool wroteOutput = false;

    for (const auto& statement : statements) {
        impl_->parser_.setLanguageStyle(impl_->languageStyle_);
        auto ast = impl_->parser_.parse(statement.text);
        if (impl_->parser_.hasError() || !ast) {
            const std::size_t pos = statement.offset + impl_->parser_.getErrorPosition();
            impl_->fail(result,
                        impl_->parser_.getError(),
                        pos,
                        impl_->parser_.getErrorLength(),
                        source);
            return result;
        }

        auto value = impl_->evaluator_.evaluateAST(ast);
        if (impl_->evaluator_.hasError()) {
            impl_->fail(result,
                        impl_->evaluator_.getError(),
                        statement.offset,
                        statement.text.size(),
                        source);
            return result;
        }

        if (!value.isNull()) {
            if (wroteOutput) {
                output << '\n';
            }
            output << value.toString();
            wroteOutput = true;
        }
    }

    result.success = true;
    result.output = output.str();
    result.error.clear();
    result.errorPosition = std::string::npos;
    result.errorLength = 0;
    result.errorLine = 0;
    result.errorColumn = 0;
    impl_->lastResult_ = result;
    return result;
}

ScriptExecutionResult ScriptRuntime::executeFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        ScriptExecutionResult result;
        impl_->fail(result, "Cannot open script file: " + path.string(), std::string::npos, 0, {});
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return execute(buffer.str());
}

ScriptExecutionResult ScriptRuntime::lastResult() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->lastResult_;
}

std::string ScriptRuntime::lastError() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->lastResult_.error;
}

bool ScriptRuntime::hasError() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return !impl_->lastResult_.success && !impl_->lastResult_.error.empty();
}

void ScriptRuntime::clearError()
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->lastResult_ = ScriptExecutionResult{};
}

void ScriptRuntime::reset()
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->hostSnapshot_ = ScriptHostSnapshot{};
    impl_->lastResult_ = ScriptExecutionResult{};
    impl_->parser_.setLanguageStyle(impl_->languageStyle_);
    impl_->evaluator_.clearVariables();
}

} // namespace ArtifactCore
