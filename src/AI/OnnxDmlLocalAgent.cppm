module;
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <QRegularExpression>
#include <sentencepiece_processor.h>
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_c_api.h>
#include <onnxruntime/dml_provider_factory.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <algorithm>
#include <numeric>

module Core.AI.OnnxDmlAgent;

import std;
import Core.AI.Context;

namespace ArtifactCore {

namespace {

QString toQString(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

std::string toUtf8(const QString& s)
{
    const QByteArray utf8 = s.toUtf8();
    return std::string(utf8.constData(), static_cast<size_t>(utf8.size()));
}

QString joinPath(const QString& base, const QString& child)
{
    return QDir(base).filePath(child);
}

QStringList candidateOnnxFiles(const QString& path)
{
    QStringList candidates;
    QFileInfo info(path);
    if (info.isDir()) {
        const QDir dir(info.absoluteFilePath());
        const QStringList filters{QStringLiteral("*.onnx"), QStringLiteral("*.ort")};
        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const auto& file : files) {
            candidates.push_back(file.absoluteFilePath());
        }
        candidates.push_back(joinPath(info.absoluteFilePath(), QStringLiteral("model.onnx")));
        candidates.push_back(joinPath(info.absoluteFilePath(), QStringLiteral("decoder_model.onnx")));
        candidates.push_back(joinPath(info.absoluteFilePath(), QStringLiteral("lm.onnx")));
    } else {
        candidates.push_back(info.absoluteFilePath());
        if (info.suffix().compare(QStringLiteral("onnx"), Qt::CaseInsensitive) == 0) {
            candidates.push_back(info.absolutePath());
        }
    }
    candidates.removeDuplicates();
    return candidates;
}

QString locateTokenizerModel(const QFileInfo& modelInfo)
{
    const QDir dir = modelInfo.isDir() ? QDir(modelInfo.absoluteFilePath()) : QDir(modelInfo.absolutePath());
    const QStringList names{
        QStringLiteral("tokenizer.model"),
        QStringLiteral("spiece.model"),
        QStringLiteral("sentencepiece.model")
    };
    for (const auto& name : names) {
        const QString candidate = dir.filePath(name);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString buildPrompt(const QString& systemPrompt, const QString& userPrompt, const AIContext& context)
{
    QStringList parts;
    if (!systemPrompt.trimmed().isEmpty()) {
        parts << QStringLiteral("System:\n%1").arg(systemPrompt.trimmed());
    }
    if (!context.projectSummary().isEmpty()) {
        parts << QStringLiteral("Project:\n%1").arg(context.projectSummary().trimmed());
    }
    if (!context.activeCompositionId().isEmpty()) {
        parts << QStringLiteral("Composition: %1").arg(context.activeCompositionId());
    }
    const auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        QStringList layers;
        for (const auto& layer : selectedLayers) {
            layers.push_back(layer);
        }
        parts << QStringLiteral("Selected layers: %1").arg(layers.join(QStringLiteral(", ")));
    }
    parts << QStringLiteral("User:\n%1").arg(userPrompt.trimmed());
    parts << QStringLiteral("Assistant:");
    return parts.join(QStringLiteral("\n\n"));
}

int32_t argmax(const float* data, size_t count)
{
    if (!data || count == 0) {
        return -1;
    }
    size_t best = 0;
    float bestValue = data[0];
    for (size_t i = 1; i < count; ++i) {
        if (data[i] > bestValue) {
            bestValue = data[i];
            best = i;
        }
    }
    return static_cast<int32_t>(best);
}

} // namespace

class OnnxDmlLocalAgent::Impl {
public:
    QString lastErrorMessage;
    QString modelPath;
    QString tokenizerPath;
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<sentencepiece::SentencePieceProcessor> tokenizer;
    std::vector<std::string> inputNamesStorage;
    std::vector<std::string> outputNamesStorage;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    bool initialized = false;
    int maxTokens = 128;
    float temperature = 0.0f;
    int eosId = -1;
    int bosId = -1;
    int unkId = -1;

    static QString readError(const Ort::Exception& e)
    {
        return QString::fromUtf8(e.what());
    }

    void rebuildNameCache()
    {
        inputNamesStorage.clear();
        outputNamesStorage.clear();
        inputNames.clear();
        outputNames.clear();
        if (!session) {
            return;
        }

        Ort::AllocatorWithDefaultOptions allocator;
        const size_t inputCount = session->GetInputCount();
        inputNamesStorage.reserve(inputCount);
        inputNames.reserve(inputCount);
        for (size_t i = 0; i < inputCount; ++i) {
            auto name = session->GetInputNameAllocated(i, allocator);
            inputNamesStorage.emplace_back(name.get());
            inputNames.push_back(inputNamesStorage.back().c_str());
        }

        const size_t outputCount = session->GetOutputCount();
        outputNamesStorage.reserve(outputCount);
        outputNames.reserve(outputCount);
        for (size_t i = 0; i < outputCount; ++i) {
            auto name = session->GetOutputNameAllocated(i, allocator);
            outputNamesStorage.emplace_back(name.get());
            outputNames.push_back(outputNamesStorage.back().c_str());
        }
    }

    QString inferModelPath(const QString& path)
    {
        const QFileInfo info(path);
        for (const auto& candidate : candidateOnnxFiles(path)) {
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
        return {};
    }
};

OnnxDmlLocalAgent::OnnxDmlLocalAgent() : impl_(std::make_unique<Impl>()) {}
OnnxDmlLocalAgent::~OnnxDmlLocalAgent() = default;

QString OnnxDmlLocalAgent::lastError() const
{
    return impl_->lastErrorMessage;
}

bool OnnxDmlLocalAgent::initialize(const QString& modelPath)
{
    impl_->lastErrorMessage.clear();
    impl_->initialized = false;

    const QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists()) {
        impl_->lastErrorMessage = QStringLiteral("Model path not found: %1").arg(modelPath);
        qWarning() << "[OnnxDmlLocalAgent]" << impl_->lastErrorMessage;
        return false;
    }

    const QString onnxPath = impl_->inferModelPath(modelPath);
    if (onnxPath.isEmpty()) {
        impl_->lastErrorMessage = QStringLiteral("No ONNX model file found at: %1").arg(modelPath);
        qWarning() << "[OnnxDmlLocalAgent]" << impl_->lastErrorMessage;
        return false;
    }

    const QString tokenizerPath = locateTokenizerModel(QFileInfo(onnxPath));
    if (tokenizerPath.isEmpty()) {
        impl_->lastErrorMessage = QStringLiteral(
            "No SentencePiece tokenizer found next to ONNX model: %1"
        ).arg(onnxPath);
        qWarning() << "[OnnxDmlLocalAgent]" << impl_->lastErrorMessage;
        return false;
    }

    try {
        impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ArtifactOnnxDmlLocalAgent");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        const int threadCount = static_cast<int>(std::thread::hardware_concurrency() / 2);
        sessionOptions.SetIntraOpNumThreads(threadCount > 2 ? threadCount : 2);
        sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        sessionOptions.SetLogSeverityLevel(3);
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));

        const std::wstring onnxWide = onnxPath.toStdWString();
        impl_->session = std::make_unique<Ort::Session>(*impl_->env, onnxWide.c_str(), sessionOptions);

        impl_->tokenizer = std::make_unique<sentencepiece::SentencePieceProcessor>();
        const std::string tokenizerUtf8 = toUtf8(tokenizerPath);
        const bool tokenizerOk = impl_->tokenizer->Load(tokenizerUtf8).ok();
        if (!tokenizerOk) {
            impl_->lastErrorMessage = QStringLiteral("Failed to load SentencePiece tokenizer: %1").arg(tokenizerPath);
            qWarning() << "[OnnxDmlLocalAgent]" << impl_->lastErrorMessage;
            impl_->session.reset();
            impl_->env.reset();
            impl_->tokenizer.reset();
            return false;
        }

        impl_->modelPath = onnxPath;
        impl_->tokenizerPath = tokenizerPath;
        impl_->rebuildNameCache();
        impl_->eosId = impl_->tokenizer->eos_id();
        impl_->bosId = impl_->tokenizer->bos_id();
        impl_->unkId = impl_->tokenizer->unk_id();
        impl_->initialized = true;

        qInfo() << "[OnnxDmlLocalAgent] initialized"
                << "model=" << onnxPath
                << "tokenizer=" << tokenizerPath
                << "inputs=" << static_cast<qulonglong>(impl_->inputNames.size())
                << "outputs=" << static_cast<qulonglong>(impl_->outputNames.size());
        return true;
    } catch (const Ort::Exception& e) {
        impl_->lastErrorMessage = QStringLiteral("ONNX Runtime init failed: %1").arg(Impl::readError(e));
    } catch (const std::exception& e) {
        impl_->lastErrorMessage = QStringLiteral("ONNX backend init failed: %1").arg(QString::fromUtf8(e.what()));
    }

    qWarning() << "[OnnxDmlLocalAgent]" << impl_->lastErrorMessage;
    impl_->session.reset();
    impl_->env.reset();
    impl_->tokenizer.reset();
    return false;
}

QString OnnxDmlLocalAgent::analyzeContext(const AIContext& context)
{
    auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        return QStringLiteral("選択中のレイヤー：%1").arg(selectedLayers.front());
    }
    if (!context.activeCompositionId().isEmpty()) {
        return QStringLiteral("アクティブコンポジション：%1").arg(context.activeCompositionId());
    }
    return QStringLiteral("コンテキスト情報なし");
}

QString OnnxDmlLocalAgent::predictParameter(const QString& targetProperty, const AIContext& context)
{
    Q_UNUSED(targetProperty);
    Q_UNUSED(context);
    return QStringLiteral("{\"value\": 0.5}");
}

bool OnnxDmlLocalAgent::requiresCloudEscalation(const QString& userPrompt, const AIContext& context)
{
    Q_UNUSED(context);
    const QString lowered = userPrompt.toLower();
    return lowered.contains(QStringLiteral("script")) ||
           lowered.contains(QStringLiteral("algorithm")) ||
           lowered.contains(QStringLiteral("coding")) ||
           lowered.contains(QStringLiteral("映画")) ||
           lowered.contains(QStringLiteral("video")) ||
           lowered.contains(QStringLiteral("render"));
}

LocalAnalysisResult OnnxDmlLocalAgent::analyzeUserQuestion(const QString& question, const AIContext& context)
{
    LocalAnalysisResult result;
    result.requiresCloud = requiresCloudEscalation(question, context);
    result.intent = result.requiresCloud ? QStringLiteral("unknown") : QStringLiteral("analysis");
    result.confidence = 0.5f;
    result.localAnswer = result.requiresCloud
        ? QStringLiteral("より詳細な推論が必要です。")
        : QStringLiteral("ローカル ONNX backend で処理できます。");
    result.summarizedContext = analyzeContext(context);
    return result;
}

QString OnnxDmlLocalAgent::filterSensitiveInfo(const QString& text)
{
    return text;
}

QString OnnxDmlLocalAgent::generateChatResponse(
    const QString& systemPrompt,
    const QString& userPrompt,
    const AIContext& context)
{
    QString response;
    generateChatResponseStreaming(
        systemPrompt,
        userPrompt,
        context,
        [&response](const QString& piece) {
            response += piece;
            return true;
        });
    return response;
}

QString OnnxDmlLocalAgent::generateChatResponseStreaming(
    const QString& systemPrompt,
    const QString& userPrompt,
    const AIContext& context,
    const std::function<bool(const QString& piece)>& tokenCallback)
{
    if (!impl_->initialized || !impl_->session || !impl_->tokenizer) {
        impl_->lastErrorMessage = QStringLiteral("ONNX backend is not initialized.");
        return {};
    }

    QString fullPrompt = buildPrompt(systemPrompt, userPrompt, context);
    std::vector<int> promptIds;
    if (!impl_->tokenizer->Encode(toUtf8(fullPrompt), &promptIds).ok() || promptIds.empty()) {
        impl_->lastErrorMessage = QStringLiteral("Failed to tokenize prompt for ONNX backend.");
        return {};
    }

    std::vector<int> generatedIds;
    QString previousGeneratedText;
    const int maxNewTokens = impl_->maxTokens > 16 ? impl_->maxTokens : 16;

    for (int step = 0; step < maxNewTokens; ++step) {
        std::vector<int64_t> inputIds(promptIds.begin(), promptIds.end());
        if (!generatedIds.empty()) {
            inputIds.insert(inputIds.end(), generatedIds.begin(), generatedIds.end());
        }

        std::vector<int64_t> dims{1, static_cast<int64_t>(inputIds.size())};
        Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
            memInfo,
            inputIds.data(),
            inputIds.size(),
            dims.data(),
            dims.size());

        std::vector<Ort::Value> ortInputs;
        std::vector<std::string> runtimeInputNames;
        std::vector<const char*> inputNames;
        bool hasUnsupportedCacheInput = false;
        for (const auto& storedName : impl_->inputNamesStorage) {
            const QString name = QString::fromUtf8(storedName);
            if (name == QStringLiteral("input_ids")) {
                ortInputs.push_back(std::move(inputTensor));
                runtimeInputNames.push_back(storedName);
                inputNames.push_back(runtimeInputNames.back().c_str());
            } else if (name == QStringLiteral("attention_mask")) {
                std::vector<int64_t> attentionMask(inputIds.size(), 1);
                Ort::Value maskTensor = Ort::Value::CreateTensor<int64_t>(
                    memInfo,
                    attentionMask.data(),
                    attentionMask.size(),
                    dims.data(),
                    dims.size());
                ortInputs.push_back(std::move(maskTensor));
                runtimeInputNames.push_back(storedName);
                inputNames.push_back(runtimeInputNames.back().c_str());
            } else if (name == QStringLiteral("position_ids")) {
                std::vector<int64_t> positionIds(inputIds.size());
                std::iota(positionIds.begin(), positionIds.end(), 0);
                Ort::Value posTensor = Ort::Value::CreateTensor<int64_t>(
                    memInfo,
                    positionIds.data(),
                    positionIds.size(),
                    dims.data(),
                    dims.size());
                ortInputs.push_back(std::move(posTensor));
                runtimeInputNames.push_back(storedName);
                inputNames.push_back(runtimeInputNames.back().c_str());
            } else if (name == QStringLiteral("token_type_ids")) {
                std::vector<int64_t> tokenTypeIds(inputIds.size(), 0);
                Ort::Value typeTensor = Ort::Value::CreateTensor<int64_t>(
                    memInfo,
                    tokenTypeIds.data(),
                    tokenTypeIds.size(),
                    dims.data(),
                    dims.size());
                ortInputs.push_back(std::move(typeTensor));
                runtimeInputNames.push_back(storedName);
                inputNames.push_back(runtimeInputNames.back().c_str());
            } else if (name.contains(QStringLiteral("past")) || name.contains(QStringLiteral("cache"))) {
                hasUnsupportedCacheInput = true;
            }
        }

        if (hasUnsupportedCacheInput) {
            impl_->lastErrorMessage = QStringLiteral(
                "ONNX model uses KV-cache inputs, which this fallback backend does not support yet."
            );
            return {};
        }

        if (ortInputs.empty()) {
            ortInputs.push_back(std::move(inputTensor));
            runtimeInputNames.emplace_back("input_ids");
            inputNames.push_back(runtimeInputNames.back().c_str());
        }

        const char* outputName = impl_->outputNames.empty() ? "logits" : impl_->outputNames.front();

        std::vector<const char*> outputNames{outputName};
        auto outputs = impl_->session->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            ortInputs.data(),
            ortInputs.size(),
            outputNames.data(),
            outputNames.size());

        if (outputs.empty() || !outputs.front().IsTensor()) {
            impl_->lastErrorMessage = QStringLiteral("ONNX backend returned no tensor output.");
            return {};
        }

        auto info = outputs.front().GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        if (shape.empty()) {
            impl_->lastErrorMessage = QStringLiteral("ONNX logits tensor has an invalid shape.");
            return {};
        }

        const float* logits = outputs.front().GetTensorData<float>();
        size_t vocabSize = 0;
        size_t lastTokenOffset = 0;
        if (shape.size() >= 2) {
            vocabSize = static_cast<size_t>(shape.back());
            size_t seqLen = static_cast<size_t>(shape[shape.size() - 2]);
            if (seqLen > 0) {
                lastTokenOffset = (seqLen - 1) * vocabSize;
            }
        } else {
            vocabSize = static_cast<size_t>(shape.back());
        }
        if (!logits || vocabSize == 0) {
            impl_->lastErrorMessage = QStringLiteral("ONNX logits tensor is empty.");
            return {};
        }

        const int32_t nextId = argmax(logits + lastTokenOffset, vocabSize);
        if (nextId < 0) {
            break;
        }
        if ((impl_->eosId >= 0 && nextId == impl_->eosId) ||
            (impl_->unkId >= 0 && nextId == impl_->unkId)) {
            break;
        }

        generatedIds.push_back(static_cast<int>(nextId));

        std::string suffix;
        if (!impl_->tokenizer->Decode(generatedIds, &suffix).ok()) {
            impl_->lastErrorMessage = QStringLiteral("Failed to decode generated token stream.");
            return {};
        }
        QString suffixText = QString::fromUtf8(suffix);
        if (suffixText.startsWith(QChar(0x2581))) {
            suffixText.remove(0, 1);
            suffixText.prepend(QChar::fromLatin1(' '));
        }
        const QString newPiece = suffixText.mid(previousGeneratedText.size());
        previousGeneratedText = suffixText;
        if (!newPiece.isEmpty() && tokenCallback) {
            if (!tokenCallback(newPiece)) {
                break;
            }
        }
    }

    std::string finalText;
    if (!generatedIds.empty()) {
        if (!impl_->tokenizer->Decode(generatedIds, &finalText).ok()) {
            impl_->lastErrorMessage = QStringLiteral("Failed to decode ONNX response.");
            return {};
        }
    }
    QString response = QString::fromUtf8(finalText);
    if (response.startsWith(QStringLiteral("\u2581"))) {
        response.remove(0, 1);
        response.prepend(QChar::fromLatin1(' '));
    }
    return response.trimmed();
}

} // namespace ArtifactCore
