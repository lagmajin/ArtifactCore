module;
#include <algorithm>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>

export module Core.AI.CommandSandbox;

import std;
import Core.AI.Describable;

export namespace ArtifactCore {

struct CommandSandboxResult {
    bool allowed = false;
    bool started = false;
    bool finished = false;
    bool timedOut = false;
    bool outputTruncated = false;
    bool ok = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    qint64 elapsedMs = 0;
    QString requestedProgram;
    QString resolvedProgram;
    QStringList arguments;
    QString workingDirectory;
    QString stdoutText;
    QString stderrText;
    QString errorText;

    QJsonObject toJson() const
    {
        QJsonObject json;
        json[QStringLiteral("allowed")] = allowed;
        json[QStringLiteral("started")] = started;
        json[QStringLiteral("finished")] = finished;
        json[QStringLiteral("timedOut")] = timedOut;
        json[QStringLiteral("outputTruncated")] = outputTruncated;
        json[QStringLiteral("ok")] = ok;
        json[QStringLiteral("exitCode")] = exitCode;
        json[QStringLiteral("exitStatus")] =
            exitStatus == QProcess::NormalExit ? QStringLiteral("NormalExit")
                                               : QStringLiteral("CrashExit");
        json[QStringLiteral("elapsedMs")] = static_cast<double>(elapsedMs);
        json[QStringLiteral("requestedProgram")] = requestedProgram;
        json[QStringLiteral("resolvedProgram")] = resolvedProgram;
        QJsonArray args;
        for (const QString& argument : arguments) {
            args.append(argument);
        }
        json[QStringLiteral("arguments")] = args;
        json[QStringLiteral("workingDirectory")] = workingDirectory;
        json[QStringLiteral("stdout")] = stdoutText;
        json[QStringLiteral("stderr")] = stderrText;
        json[QStringLiteral("error")] = errorText;
        return json;
    }

    QVariantMap toVariantMap() const
    {
        return toJson().toVariantMap();
    }
};

struct CommandSandboxPolicy {
    QStringList allowedPrograms;
    QStringList blockedPrograms;
    QStringList allowedEnvironmentKeys;
    QString workingDirectory;
    int defaultTimeoutMs = 5000;
    int maxOutputBytes = 131072;
    bool allowShellPrograms = false;
};

class CommandSandbox : public IDescribable {
public:
    CommandSandbox()
    {
        resetPolicy();
    }

    static CommandSandbox& instance()
    {
        static CommandSandbox sandbox;
        return sandbox;
    }

    static void ensureRegistered()
    {
        static const bool registered = []() {
            DescriptionRegistry::instance().registerDescribable(
                QStringLiteral("CommandSandbox"),
                []() -> const IDescribable* {
                    return &CommandSandbox::instance();
                });
            return true;
        }();
        (void)registered;
    }

    QString className() const override { return QStringLiteral("CommandSandbox"); }

    LocalizedText briefDescription() const override
    {
        return loc(
            "Runs explicit command-line tools under a narrow allowlist policy.",
            "Runs explicit command-line tools under a narrow allowlist policy.",
            {});
    }

    LocalizedText detailedDescription() const override
    {
        return loc(
            "Commands are executed as program + argv without shell parsing. "
            "The sandbox validates the program name, applies a working-directory "
            "constraint, limits captured output, and rejects shell interpreters "
            "unless they are explicitly enabled.",
            "Commands are executed as program + argv without shell parsing. "
            "The sandbox validates the program name, applies a working-directory "
            "constraint, limits captured output, and rejects shell interpreters "
            "unless they are explicitly enabled.",
            {});
    }

    QList<PropertyDescription> propertyDescriptions() const override
    {
        return {
            {"allowedPrograms", loc("Programs allowed to run.", "Programs allowed to run.", {}), "QStringList"},
            {"blockedPrograms", loc("Programs that are always rejected.", "Programs that are always rejected.", {}), "QStringList"},
            {"allowedEnvironmentKeys", loc("Environment variables copied into the child process.", "Environment variables copied into the child process.", {}), "QStringList"},
            {"workingDirectory", loc("Fixed working directory for command execution.", "Fixed working directory for command execution.", {}), "QString"},
            {"defaultTimeoutMs", loc("Default timeout used for execution.", "Default timeout used for execution.", {}), "int", "5000"},
            {"maxOutputBytes", loc("Maximum amount of stdout/stderr captured.", "Maximum amount of stdout/stderr captured.", {}), "int", "131072"},
            {"allowShellPrograms", loc("Whether shell interpreters are allowed.", "Whether shell interpreters are allowed.", {}), "bool", "false"}
        };
    }

    QList<MethodDescription> methodDescriptions() const override
    {
        return {
            {"resetPolicy",
             loc("Restore the default sandbox policy.", "Restore the default sandbox policy.", {}),
             "bool"},
            {"setAllowedPrograms",
             loc("Replace the allowed program list.", "Replace the allowed program list.", {}),
             "bool",
             {QStringLiteral("QStringList")},
             {QStringLiteral("programs")}},
            {"setBlockedPrograms",
             loc("Replace the blocked program list.", "Replace the blocked program list.", {}),
             "bool",
             {QStringLiteral("QStringList")},
             {QStringLiteral("programs")}},
            {"setAllowedEnvironmentKeys",
             loc("Replace the environment key allowlist.", "Replace the environment key allowlist.", {}),
             "bool",
             {QStringLiteral("QStringList")},
             {QStringLiteral("keys")}},
            {"setWorkingDirectory",
             loc("Set the sandbox working directory.", "Set the sandbox working directory.", {}),
             "bool",
             {QStringLiteral("QString")},
             {QStringLiteral("path")}},
            {"setDefaultTimeoutMs",
             loc("Set the default execution timeout.", "Set the default execution timeout.", {}),
             "bool",
             {QStringLiteral("int")},
             {QStringLiteral("timeoutMs")}},
            {"setMaxOutputBytes",
             loc("Set the maximum captured output size.", "Set the maximum captured output size.", {}),
             "bool",
             {QStringLiteral("int")},
             {QStringLiteral("byteCount")}},
            {"setAllowShellPrograms",
             loc("Enable or disable shell interpreters.", "Enable or disable shell interpreters.", {}),
             "bool",
             {QStringLiteral("bool")},
             {QStringLiteral("allow")}},
            {"policy",
             loc("Return the current sandbox policy.", "Return the current sandbox policy.", {}),
             "QVariantMap"},
            {"dryRun",
             loc("Validate a command without executing it.", "Validate a command without executing it.", {}),
             "QVariantMap",
             {QStringLiteral("QString"), QStringLiteral("QVariantList")},
             {QStringLiteral("program"), QStringLiteral("arguments")}},
            {"run",
             loc("Execute an allowed command and capture its output.", "Execute an allowed command and capture its output.", {}),
             "QVariantMap",
             {QStringLiteral("QString"), QStringLiteral("QVariantList")},
             {QStringLiteral("program"), QStringLiteral("arguments")}}
        };
    }

    QVariant invokeMethod(QStringView name, const QVariantList& args) override
    {
        if (name == QStringLiteral("resetPolicy")) {
            resetPolicy();
            return true;
        }
        if (name == QStringLiteral("setAllowedPrograms")) {
            setAllowedPrograms(collectStringList(args));
            return true;
        }
        if (name == QStringLiteral("setBlockedPrograms")) {
            setBlockedPrograms(collectStringList(args));
            return true;
        }
        if (name == QStringLiteral("setAllowedEnvironmentKeys")) {
            setAllowedEnvironmentKeys(collectStringList(args));
            return true;
        }
        if (name == QStringLiteral("setWorkingDirectory")) {
            if (!args.isEmpty()) {
                setWorkingDirectory(args.first().toString());
            }
            return true;
        }
        if (name == QStringLiteral("setDefaultTimeoutMs")) {
            if (!args.isEmpty()) {
                setDefaultTimeoutMs(args.first().toInt());
            }
            return true;
        }
        if (name == QStringLiteral("setMaxOutputBytes")) {
            if (!args.isEmpty()) {
                setMaxOutputBytes(args.first().toInt());
            }
            return true;
        }
        if (name == QStringLiteral("setAllowShellPrograms")) {
            if (!args.isEmpty()) {
                setAllowShellPrograms(args.first().toBool());
            }
            return true;
        }
        if (name == QStringLiteral("policy")) {
            return policyJson().toVariantMap();
        }
        if (name == QStringLiteral("dryRun")) {
            const QString program = args.isEmpty() ? QString() : args.first().toString();
            const QVariantList commandArgs = args.size() > 1 ? collectCommandArgs(args.mid(1)) : QVariantList{};
            return dryRun(program, commandArgs);
        }
        if (name == QStringLiteral("run")) {
            const QString program = args.isEmpty() ? QString() : args.first().toString();
            const QVariantList commandArgs = args.size() > 1 ? collectCommandArgs(args.mid(1)) : QVariantList{};
            return run(program, commandArgs);
        }
        return {};
    }

    void resetPolicy()
    {
        policy_ = CommandSandboxPolicy{};
        policy_.allowedPrograms = defaultAllowedPrograms();
        policy_.allowedEnvironmentKeys = defaultAllowedEnvironmentKeys();
    }

    void setAllowedPrograms(const QStringList& programs)
    {
        policy_.allowedPrograms = sanitizeList(programs);
    }

    void setBlockedPrograms(const QStringList& programs)
    {
        policy_.blockedPrograms = sanitizeList(programs);
    }

    void setAllowedEnvironmentKeys(const QStringList& keys)
    {
        policy_.allowedEnvironmentKeys = sanitizeList(keys);
    }

    void setWorkingDirectory(const QString& path)
    {
        policy_.workingDirectory = path.trimmed();
    }

    void setDefaultTimeoutMs(int timeoutMs)
    {
        policy_.defaultTimeoutMs = std::max(1, timeoutMs);
    }

    void setMaxOutputBytes(int byteCount)
    {
        policy_.maxOutputBytes = std::max(1024, byteCount);
    }

    void setAllowShellPrograms(bool allow)
    {
        policy_.allowShellPrograms = allow;
    }

    QJsonObject policyJson() const
    {
        QJsonObject json;
        json[QStringLiteral("allowedPrograms")] = toJsonArray(policy_.allowedPrograms);
        json[QStringLiteral("blockedPrograms")] = toJsonArray(policy_.blockedPrograms);
        json[QStringLiteral("allowedEnvironmentKeys")] = toJsonArray(policy_.allowedEnvironmentKeys);
        json[QStringLiteral("workingDirectory")] = policy_.workingDirectory;
        json[QStringLiteral("defaultTimeoutMs")] = policy_.defaultTimeoutMs;
        json[QStringLiteral("maxOutputBytes")] = policy_.maxOutputBytes;
        json[QStringLiteral("allowShellPrograms")] = policy_.allowShellPrograms;
        return json;
    }

    QVariant dryRun(const QString& requestedProgram, const QVariantList& arguments) const
    {
        const CommandExecutionPlan plan = preparePlan(requestedProgram, arguments);
        return plan.toResult(false).toVariantMap();
    }

    QVariant run(const QString& requestedProgram, const QVariantList& arguments) const
    {
        const CommandExecutionPlan plan = preparePlan(requestedProgram, arguments);
        CommandSandboxResult result = plan.toResult(true);
        if (!plan.allowed) {
            return result.toVariantMap();
        }

        QProcess process;
        process.setProgram(plan.resolvedProgram);
        process.setArguments(plan.arguments);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.setWorkingDirectory(plan.workingDirectory);
        process.setProcessEnvironment(buildEnvironment());

        QElapsedTimer timer;
        timer.start();
        process.start();
        if (!process.waitForStarted(policy_.defaultTimeoutMs)) {
            result.started = false;
            result.errorText = process.errorString();
            result.elapsedMs = timer.elapsed();
            return result.toVariantMap();
        }
        result.started = true;

        QByteArray stdoutBytes;
        QByteArray stderrBytes;
        bool outputTruncated = false;
        const qint64 timeoutMs = std::max(1, policy_.defaultTimeoutMs);

        while (true) {
            if (process.waitForReadyRead(50)) {
                appendLimited(stdoutBytes, process.readAllStandardOutput(), policy_.maxOutputBytes, &outputTruncated);
                appendLimited(stderrBytes, process.readAllStandardError(), policy_.maxOutputBytes, &outputTruncated);
            }

            if (process.state() == QProcess::NotRunning) {
                break;
            }

            if (timer.elapsed() >= timeoutMs) {
                result.timedOut = true;
                process.kill();
                process.waitForFinished(1000);
                break;
            }
        }

        appendLimited(stdoutBytes, process.readAllStandardOutput(), policy_.maxOutputBytes, &outputTruncated);
        appendLimited(stderrBytes, process.readAllStandardError(), policy_.maxOutputBytes, &outputTruncated);

        result.finished = process.state() == QProcess::NotRunning;
        result.outputTruncated = outputTruncated;
        result.exitCode = process.exitCode();
        result.exitStatus = process.exitStatus();
        result.stdoutText = QString::fromUtf8(stdoutBytes);
        result.stderrText = QString::fromUtf8(stderrBytes);
        result.elapsedMs = timer.elapsed();
        result.ok = result.allowed && result.started && result.finished && !result.timedOut &&
                    result.exitStatus == QProcess::NormalExit && result.exitCode == 0;
        if (!result.ok && result.errorText.isEmpty()) {
            if (result.timedOut) {
                result.errorText = QStringLiteral("Command timed out");
            } else if (result.exitStatus != QProcess::NormalExit) {
                result.errorText = QStringLiteral("Command crashed");
            } else if (result.exitCode != 0) {
                result.errorText = QStringLiteral("Command exited with code %1").arg(result.exitCode);
            }
        }
        return result.toVariantMap();
    }

private:
    struct CommandExecutionPlan {
        bool allowed = false;
        QString requestedProgram;
        QString resolvedProgram;
        QStringList arguments;
        QString workingDirectory;
        QString errorText;

        CommandSandboxResult toResult(bool executionAttempted) const
        {
            CommandSandboxResult result;
            result.allowed = allowed;
            result.requestedProgram = requestedProgram;
            result.resolvedProgram = resolvedProgram;
            result.arguments = arguments;
            result.workingDirectory = workingDirectory;
            result.errorText = errorText;
            result.started = executionAttempted && allowed;
            return result;
        }
    };

    static QJsonArray toJsonArray(const QStringList& values)
    {
        QJsonArray json;
        for (const QString& value : values) {
            json.append(value);
        }
        return json;
    }

    static QStringList defaultAllowedPrograms()
    {
        return {
            QStringLiteral("git"),
            QStringLiteral("git.exe"),
            QStringLiteral("cmake"),
            QStringLiteral("cmake.exe"),
            QStringLiteral("ctest"),
            QStringLiteral("ctest.exe"),
            QStringLiteral("ninja"),
            QStringLiteral("ninja.exe"),
            QStringLiteral("rg"),
            QStringLiteral("rg.exe"),
            QStringLiteral("where"),
            QStringLiteral("where.exe")
        };
    }

    static QStringList defaultAllowedEnvironmentKeys()
    {
        return {
            QStringLiteral("PATH"),
            QStringLiteral("SystemRoot"),
            QStringLiteral("WINDIR"),
            QStringLiteral("TEMP"),
            QStringLiteral("TMP"),
            QStringLiteral("HOME"),
            QStringLiteral("USERPROFILE"),
            QStringLiteral("LANG"),
            QStringLiteral("LANGUAGE"),
            QStringLiteral("LC_ALL")
        };
    }

    static QStringList sanitizeList(const QStringList& values)
    {
        QStringList sanitized;
        for (const QString& value : values) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty()) {
                sanitized.append(trimmed);
            }
        }
        sanitized.removeDuplicates();
        return sanitized;
    }

    static QStringList collectStringList(const QVariantList& args)
    {
        if (args.isEmpty()) {
            return {};
        }

        if (args.size() == 1) {
            const QVariant& first = args.first();
            const QVariantList nested = first.toList();
            if (!nested.isEmpty()) {
                QStringList values;
                for (const QVariant& value : nested) {
                    values.append(value.toString());
                }
                return sanitizeList(values);
            }

            const QStringList direct = first.toStringList();
            if (!direct.isEmpty()) {
                return sanitizeList(direct);
            }
        }

        QStringList values;
        for (const QVariant& value : args) {
            values.append(value.toString());
        }
        return sanitizeList(values);
    }

    static QVariantList collectCommandArgs(const QVariantList& args)
    {
        if (args.isEmpty()) {
            return {};
        }
        if (args.size() == 1) {
            const QVariantList nested = args.first().toList();
            if (!nested.isEmpty()) {
                return nested;
            }
        }
        return args;
    }

    static bool isShellProgramName(const QString& program)
    {
        const QString key = QFileInfo(program).fileName().trimmed().toLower();
        static const QSet<QString> shellNames = {
            QStringLiteral("cmd"),
            QStringLiteral("cmd.exe"),
            QStringLiteral("powershell"),
            QStringLiteral("powershell.exe"),
            QStringLiteral("pwsh"),
            QStringLiteral("pwsh.exe"),
            QStringLiteral("sh"),
            QStringLiteral("bash"),
            QStringLiteral("zsh"),
            QStringLiteral("fish")
        };
        return shellNames.contains(key);
    }

    static QString normalizePath(const QString& path)
    {
        if (path.trimmed().isEmpty()) {
            return {};
        }
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    static bool matchesAllowedProgram(const QString& resolvedProgram,
                                      const QString& requestedProgram,
                                      const QStringList& allowedPrograms)
    {
        const QString resolvedKey = normalizePath(resolvedProgram).toLower();
        const QString requestedKey = QFileInfo(requestedProgram).fileName().trimmed().toLower();
        for (const QString& allowed : allowedPrograms) {
            const QString trimmedAllowed = allowed.trimmed();
            if (trimmedAllowed.isEmpty()) {
                continue;
            }
            if (QFileInfo(trimmedAllowed).isAbsolute()) {
                if (normalizePath(trimmedAllowed).toLower() == resolvedKey) {
                    return true;
                }
            } else {
                const QString allowedKey = QFileInfo(trimmedAllowed).fileName().trimmed().toLower();
                if (allowedKey == requestedKey || allowedKey == resolvedKey || trimmedAllowed.toLower() == requestedKey) {
                    return true;
                }
            }
        }
        return false;
    }

    CommandExecutionPlan preparePlan(const QString& requestedProgram, const QVariantList& arguments) const
    {
        CommandExecutionPlan plan;
        plan.requestedProgram = requestedProgram.trimmed();
        plan.arguments.reserve(arguments.size());
        for (const QVariant& value : arguments) {
            plan.arguments.append(value.toString());
        }

        if (plan.requestedProgram.isEmpty()) {
            plan.errorText = QStringLiteral("Command program is empty");
            return plan;
        }

        if (isShellProgramName(plan.requestedProgram) && !policy_.allowShellPrograms) {
            plan.errorText = QStringLiteral("Shell programs are disabled by policy: %1").arg(plan.requestedProgram);
            return plan;
        }

        QString resolvedProgram = plan.requestedProgram;
        if (!QFileInfo(resolvedProgram).isAbsolute()) {
            const QString found = QStandardPaths::findExecutable(resolvedProgram);
            if (!found.isEmpty()) {
                resolvedProgram = found;
            }
        }

        if (resolvedProgram.isEmpty()) {
            plan.errorText = QStringLiteral("Unable to resolve executable: %1").arg(plan.requestedProgram);
            return plan;
        }
        if (QFileInfo(resolvedProgram).isAbsolute() && !QFileInfo(resolvedProgram).exists()) {
            plan.errorText = QStringLiteral("Executable does not exist: %1").arg(resolvedProgram);
            return plan;
        }

        if (!policy_.blockedPrograms.isEmpty() &&
            matchesAllowedProgram(resolvedProgram, plan.requestedProgram, policy_.blockedPrograms)) {
            plan.errorText = QStringLiteral("Program is blocked by policy: %1").arg(plan.requestedProgram);
            return plan;
        }

        if (!policy_.allowedPrograms.isEmpty() &&
            !matchesAllowedProgram(resolvedProgram, plan.requestedProgram, policy_.allowedPrograms)) {
            plan.errorText = QStringLiteral("Program is not on the allowlist: %1").arg(plan.requestedProgram);
            return plan;
        }

        const QString workingDirectory =
            policy_.workingDirectory.trimmed().isEmpty() ? QDir::currentPath()
                                                         : normalizePath(policy_.workingDirectory);
        if (!QFileInfo(workingDirectory).isDir()) {
            plan.errorText = QStringLiteral("Working directory does not exist: %1").arg(workingDirectory);
            return plan;
        }

        plan.allowed = true;
        plan.resolvedProgram = normalizePath(resolvedProgram);
        plan.workingDirectory = workingDirectory;
        return plan;
    }

    QProcessEnvironment buildEnvironment() const
    {
        QProcessEnvironment source = QProcessEnvironment::systemEnvironment();
        QProcessEnvironment filtered;
        const QStringList keys = policy_.allowedEnvironmentKeys.isEmpty()
                                     ? defaultAllowedEnvironmentKeys()
                                     : policy_.allowedEnvironmentKeys;
        for (const QString& key : keys) {
            if (source.contains(key)) {
                filtered.insert(key, source.value(key));
            }
        }
        if (!filtered.contains(QStringLiteral("PATH")) && source.contains(QStringLiteral("PATH"))) {
            filtered.insert(QStringLiteral("PATH"), source.value(QStringLiteral("PATH")));
        }
        return filtered;
    }

    static void appendLimited(QByteArray& target, const QByteArray& chunk, int maxBytes, bool* truncatedOut)
    {
        if (chunk.isEmpty()) {
            return;
        }
        if (target.size() >= maxBytes) {
            if (truncatedOut) {
                *truncatedOut = true;
            }
            return;
        }

        const int remaining = maxBytes - target.size();
        if (chunk.size() <= remaining) {
            target += chunk;
            return;
        }

        target += chunk.left(remaining);
        if (truncatedOut) {
            *truncatedOut = true;
        }
    }

    CommandSandboxPolicy policy_;
};

} // namespace ArtifactCore
