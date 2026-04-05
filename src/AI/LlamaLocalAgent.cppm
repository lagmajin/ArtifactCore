module;
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QFileInfo>
#include <gguf.h>
#include <llama.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <algorithm>

module Core.AI.LlamaAgent;

import std;
import Core.AI.Context;

namespace ArtifactCore {

namespace {

static std::once_flag g_llamaBackendInitOnce;

bool loadBackendIfExists(const QDir& appDir, const QString& dllName)
{
    const QString backendPath = appDir.filePath(dllName);
    if (!QFileInfo::exists(backendPath)) {
        return false;
    }

    if (!ggml_backend_load(backendPath.toUtf8().constData())) {
        qWarning() << "[LlamaLocalAgent] failed to load backend DLL:" << backendPath;
        return false;
    }

    qInfo() << "[LlamaLocalAgent] loaded backend DLL:" << backendPath;
    return true;
}

bool isCudaBackendDevice(ggml_backend_dev_t device)
{
    if (!device) {
        return false;
    }

    const ggml_backend_reg_t reg = ggml_backend_dev_backend_reg(device);
    const char* regName = reg ? ggml_backend_reg_name(reg) : nullptr;
    if (regName) {
        const QString name = QString::fromUtf8(regName).toLower();
        if (name.contains(QStringLiteral("cuda"))) {
            return true;
        }
    }

    const char* devName = ggml_backend_dev_name(device);
    if (devName) {
        const QString name = QString::fromUtf8(devName).toLower();
        if (name.contains(QStringLiteral("cuda"))) {
            return true;
        }
    }

    return false;
}

size_t findPreferredGpuDeviceIndex()
{
    constexpr size_t invalidIndex = static_cast<size_t>(-1);
    size_t firstGpuIndex = invalidIndex;

    for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
        const ggml_backend_dev_t device = ggml_backend_dev_get(i);
        if (!device) {
            continue;
        }

        const auto type = ggml_backend_dev_type(device);
        if (type != GGML_BACKEND_DEVICE_TYPE_GPU && type != GGML_BACKEND_DEVICE_TYPE_IGPU) {
            continue;
        }

        if (firstGpuIndex == invalidIndex) {
            firstGpuIndex = i;
        }

        if (isCudaBackendDevice(device)) {
            return i;
        }
    }

    return firstGpuIndex;
}

void ensureLlamaBackendInitialized()
{
    std::call_once(g_llamaBackendInitOnce, []() {
        auto logCallback = [](enum ggml_log_level level, const char* text, void*) {
            const QString message = QString::fromUtf8(text ? text : "");
            switch (level) {
            case GGML_LOG_LEVEL_ERROR:
                qWarning() << "[llama.cpp][error]" << message.trimmed();
                break;
            case GGML_LOG_LEVEL_WARN:
                qWarning() << "[llama.cpp][warn]" << message.trimmed();
                break;
            case GGML_LOG_LEVEL_INFO:
                qInfo() << "[llama.cpp][info]" << message.trimmed();
                break;
            case GGML_LOG_LEVEL_DEBUG:
                qDebug() << "[llama.cpp][debug]" << message.trimmed();
                break;
            default:
                break;
            }
        };

        ggml_log_set(
            logCallback,
            nullptr);
        llama_log_set(
            logCallback,
            nullptr);
        llama_backend_init();
        const QDir appDir(QCoreApplication::applicationDirPath());
        loadBackendIfExists(appDir, QStringLiteral("ggml-cuda.dll"));
        loadBackendIfExists(appDir, QStringLiteral("ggml-vulkan.dll"));
        loadBackendIfExists(appDir, QStringLiteral("ggml-cpu.dll"));
        const QByteArray appDirUtf8 = appDir.path().toUtf8();
        ggml_backend_load_all_from_path(appDirUtf8.constData());
        ggml_backend_load_all();
        qInfo() << "[LlamaLocalAgent] backend registry"
                << "backendCount=" << static_cast<qulonglong>(ggml_backend_reg_count())
                << "deviceCount=" << static_cast<qulonglong>(ggml_backend_dev_count());
        for (size_t i = 0; i < ggml_backend_reg_count(); ++i) {
            const ggml_backend_reg_t reg = ggml_backend_reg_get(i);
            qInfo() << "[LlamaLocalAgent] backend reg"
                    << i
                    << (reg ? ggml_backend_reg_name(reg) : "<null>");
        }
        for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
            const ggml_backend_dev_t dev = ggml_backend_dev_get(i);
            qInfo() << "[LlamaLocalAgent] backend dev"
                    << i
                    << (dev ? ggml_backend_dev_name(dev) : "<null>")
                    << (dev ? ggml_backend_dev_description(dev) : "<null>")
                    << "type=" << (dev ? ggml_backend_dev_type(dev) : GGML_BACKEND_DEVICE_TYPE_CPU);
        }
        qInfo() << "[LlamaLocalAgent] llama backend initialized";
    });
}

template <typename T, void (*FreeFn)(T*)>
struct LlamaDeleter {
    void operator()(T* ptr) const noexcept
    {
        if (ptr) {
            FreeFn(ptr);
        }
    }
};

using LlamaModelPtr = std::unique_ptr<llama_model, LlamaDeleter<llama_model, llama_model_free>>;
using LlamaContextPtr = std::unique_ptr<llama_context, LlamaDeleter<llama_context, llama_free>>;
using LlamaSamplerPtr = std::unique_ptr<llama_sampler, LlamaDeleter<llama_sampler, llama_sampler_free>>;

QString utf8ToQString(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

std::string QStringToUtf8(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<size_t>(utf8.size()));
}

size_t countGpuBackends()
{
    const size_t backendCount = ggml_backend_dev_count();
    size_t gpuCount = 0;
    for (size_t i = 0; i < backendCount; ++i) {
        const ggml_backend_dev_t device = ggml_backend_dev_get(i);
        if (device && (ggml_backend_dev_type(device) == GGML_BACKEND_DEVICE_TYPE_GPU ||
                       ggml_backend_dev_type(device) == GGML_BACKEND_DEVICE_TYPE_IGPU)) {
            ++gpuCount;
        }
    }
    return gpuCount;
}

int recommendedInferenceThreads()
{
    const unsigned int hw = std::thread::hardware_concurrency();
    if (hw <= 4) {
        return std::max(2, static_cast<int>(hw));
    }
    return std::clamp(static_cast<int>(hw / 2), 4, 8);
}

QString readGgufArchitecture(const QString& modelPath, QString* errorOut = nullptr)
{
    const std::string modelPathUtf8 = QStringToUtf8(modelPath);
    gguf_init_params params{};
    params.no_alloc = true;
    params.ctx = nullptr;

    gguf_context* raw = gguf_init_from_file(modelPathUtf8.c_str(), params);
    if (!raw) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to read GGUF metadata.");
        }
        return {};
    }

    const int64_t keyId = gguf_find_key(raw, "general.architecture");
    QString architecture;
    if (keyId >= 0) {
        const char* arch = gguf_get_val_str(raw, keyId);
        if (arch) {
            architecture = QString::fromUtf8(arch).trimmed();
        }
    }

    gguf_free(raw);
    return architecture;
}

QString buildContextSummary(const AIContext& context)
{
    QStringList lines;
    if (!context.projectSummary().isEmpty()) {
        lines.append(QStringLiteral("Project summary:\n%1").arg(context.projectSummary()));
    }
    if (!context.activeCompositionId().isEmpty()) {
        lines.append(QStringLiteral("Active composition: %1").arg(context.activeCompositionId()));
    }
    const auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        lines.append(QStringLiteral("Selected layers:"));
        for (const auto& layer : selectedLayers) {
            lines.append(QStringLiteral("- %1").arg(layer));
        }
    }
    return lines.join(QStringLiteral("\n"));
}

std::vector<llama_token> tokenizePrompt(const llama_vocab* vocab, const QString& prompt)
{
    const std::string promptUtf8 = QStringToUtf8(prompt);
    std::vector<llama_token> tokens(std::max<size_t>(64, promptUtf8.size() * 2 + 8));
    int32_t n_tokens = llama_tokenize(
        vocab,
        promptUtf8.c_str(),
        static_cast<int32_t>(promptUtf8.size()),
        tokens.data(),
        static_cast<int32_t>(tokens.size()),
        true,
        true);

    if (n_tokens < 0) {
        tokens.resize(static_cast<size_t>(-n_tokens));
        n_tokens = llama_tokenize(
            vocab,
            promptUtf8.c_str(),
            static_cast<int32_t>(promptUtf8.size()),
            tokens.data(),
            static_cast<int32_t>(tokens.size()),
            true,
            true);
    }

    if (n_tokens < 0) {
        return {};
    }

    tokens.resize(static_cast<size_t>(n_tokens));
    return tokens;
}

bool decodeTokensInChunks(
    llama_context* ctx,
    const std::vector<llama_token>& tokens,
    int batchSize,
    QString* errorOut)
{
    if (!ctx) {
        if (errorOut) {
            *errorOut = QStringLiteral("LLM context is not available.");
        }
        return false;
    }
    if (tokens.empty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Prompt tokenization produced no tokens.");
        }
        return false;
    }

    const int effectiveBatchSize = std::max(1, batchSize);
    for (size_t offset = 0; offset < tokens.size(); offset += static_cast<size_t>(effectiveBatchSize)) {
        const size_t chunkCount = std::min(tokens.size() - offset, static_cast<size_t>(effectiveBatchSize));
        llama_batch batch = llama_batch_get_one(
            const_cast<llama_token*>(tokens.data() + offset),
            static_cast<int32_t>(chunkCount));
        if (llama_decode(ctx, batch) != 0) {
            if (errorOut) {
                *errorOut = QStringLiteral("Prompt decode failed.");
            }
            return false;
        }
    }

    return true;
}

QString sampleWithModel(
    llama_model* model,
    llama_context_params cparams,
    const QString& systemPrompt,
    const QString& userPrompt,
    const AIContext& context,
    int maxTokens,
    float temperature,
    const std::function<bool(const QString&)>* tokenCallback,
    QString* errorOut)
{
    if (!model) {
        if (errorOut) {
            *errorOut = QStringLiteral("LLM model is not loaded.");
        }
        return {};
    }

    LlamaContextPtr ctx{llama_init_from_model(model, cparams)};
    if (!ctx) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to create llama context.");
        }
        return {};
    }

    const llama_vocab* vocab = llama_model_get_vocab(model);
    if (!vocab) {
        if (errorOut) {
            *errorOut = QStringLiteral("Model vocabulary is unavailable.");
        }
        return {};
    }

    const QString contextBlock = buildContextSummary(context);
    const QString effectiveSystemPrompt = systemPrompt
        + QStringLiteral("\n\nYou are an in-app assistant for ArtifactStudio. "
                         "Answer naturally in Japanese. "
                         "Use the provided context only when relevant.");
    const QString effectiveUserPrompt = contextBlock.isEmpty()
        ? userPrompt
        : QStringLiteral("%1\n\nContext:\n%2").arg(userPrompt, contextBlock);

    const std::string systemUtf8 = QStringToUtf8(effectiveSystemPrompt);
    const std::string userUtf8 = QStringToUtf8(effectiveUserPrompt);
    llama_chat_message chatMessages[2] = {
        {"system", systemUtf8.c_str()},
        {"user", userUtf8.c_str()}
    };

    std::vector<char> promptBuffer(8192);
    int32_t formattedLen = llama_chat_apply_template(
        nullptr,
        chatMessages,
        2,
        true,
        promptBuffer.data(),
        static_cast<int32_t>(promptBuffer.size()));

    if (formattedLen < 0) {
        // Fallback to a simple plain-text prompt if the model template isn't available.
        const QString fallbackPrompt = QStringLiteral("System:\n%1\n\nUser:\n%2\n\nAssistant:\n")
            .arg(effectiveSystemPrompt, effectiveUserPrompt);
        const auto tokens = tokenizePrompt(vocab, fallbackPrompt);
        if (tokens.empty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Prompt tokenization failed.");
            }
            return {};
        }
        if (!decodeTokensInChunks(ctx.get(), tokens, static_cast<int>(cparams.n_batch), errorOut)) {
            return {};
        }
    } else {
        if (formattedLen > static_cast<int32_t>(promptBuffer.size())) {
            promptBuffer.resize(static_cast<size_t>(formattedLen + 1));
            formattedLen = llama_chat_apply_template(
                nullptr,
                chatMessages,
                2,
                true,
                promptBuffer.data(),
                static_cast<int32_t>(promptBuffer.size()));
        }

        if (formattedLen <= 0) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to format chat prompt.");
            }
            return {};
        }

        const QString prompt = utf8ToQString(std::string(promptBuffer.data(), static_cast<size_t>(formattedLen)));
        const std::vector<llama_token> tokens = tokenizePrompt(vocab, prompt);
        if (tokens.empty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Prompt tokenization failed.");
            }
            return {};
        }
        if (!decodeTokensInChunks(ctx.get(), tokens, static_cast<int>(cparams.n_batch), errorOut)) {
            return {};
        }
    }

    const uint32_t seed = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    sparams.no_perf = true;
    LlamaSamplerPtr sampler{llama_sampler_chain_init(sparams)};
    if (!sampler) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to initialize sampler chain.");
        }
        return {};
    }

    if (temperature <= 0.0f) {
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_greedy());
    } else {
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_k(40));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_top_p(0.95f, 1));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(sampler.get(), llama_sampler_init_dist(seed));
    }

    QString response;
    const llama_token eos = llama_vocab_eos(vocab);
    const int effectiveMaxTokens = std::max(32, maxTokens);

    for (int i = 0; i < effectiveMaxTokens; ++i) {
        const llama_token token = llama_sampler_sample(sampler.get(), ctx.get(), -1);
        if (token == eos || llama_vocab_is_eog(vocab, token)) {
            break;
        }

        std::vector<char> pieceBuffer(256);
        int32_t pieceLen = llama_token_to_piece(
            vocab,
            token,
            pieceBuffer.data(),
            static_cast<int32_t>(pieceBuffer.size()),
            0,
            true);
        if (pieceLen < 0) {
            pieceBuffer.resize(static_cast<size_t>(-pieceLen) + 8);
            pieceLen = llama_token_to_piece(
                vocab,
                token,
                pieceBuffer.data(),
                static_cast<int32_t>(pieceBuffer.size()),
                0,
                true);
        }
        if (pieceLen > 0) {
            const QString piece = QString::fromUtf8(pieceBuffer.data(), pieceLen);
            response += piece;
            if (tokenCallback && *tokenCallback && !(*tokenCallback)(piece)) {
                break;
            }
        }

        llama_token nextToken = token;
        llama_batch batch = llama_batch_get_one(&nextToken, 1);
        if (llama_decode(ctx.get(), batch) != 0) {
            break;
        }
    }

    if (response.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Model returned an empty response.");
        }
        return {};
    }

    return response.trimmed();
}

} // namespace

class LlamaLocalAgent::Impl {
public:
    bool initialized = false;
    int n_predict = 256;
    float temp = 0.7f;
    int n_ctx = 4096;
    LlamaModelPtr model;
    QString modelPath;
    QString modelName;
    QString lastErrorMessage;
    std::mutex mutex;
    bool backendInitialized = false;
    bool gpuOffloadSupported = false;

    void ensureBackend()
    {
        if (!backendInitialized) {
            ensureLlamaBackendInitialized();
            backendInitialized = true;
        }
    }

    QString buildAnswer(const QString& systemPrompt, const QString& userPrompt, const AIContext& context, QString* errorOut = nullptr)
    {
        llama_model* modelRaw = nullptr;
        llama_context_params cparams;
        int myNPredict;
        float myTemp;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized || !model) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Local model is not initialized.");
                }
                return {};
            }
            modelRaw = model.get();
            cparams = llama_context_default_params();
            cparams.n_ctx = static_cast<uint32_t>(std::max(1024, n_ctx));
            cparams.n_batch = std::min<uint32_t>(2048, cparams.n_ctx);
            cparams.n_ubatch = std::min<uint32_t>(256, cparams.n_batch);
            cparams.n_seq_max = 1;
            cparams.n_threads = recommendedInferenceThreads();
            cparams.n_threads_batch = std::max(2, recommendedInferenceThreads() / 2);
            cparams.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_AUTO;
            cparams.offload_kqv = gpuOffloadSupported;
            cparams.no_perf = true;
            cparams.op_offload = gpuOffloadSupported;
            cparams.swa_full = false;
            cparams.kv_unified = false;
            myNPredict = n_predict;
            myTemp = temp;
        }
        return sampleWithModel(modelRaw, cparams, systemPrompt, userPrompt, context, myNPredict, myTemp, nullptr, errorOut);
    }

    QString buildAnswerStreaming(
        const QString& systemPrompt,
        const QString& userPrompt,
        const AIContext& context,
        const std::function<bool(const QString&)>& tokenCallback,
        QString* errorOut = nullptr)
    {
        llama_model* modelRaw = nullptr;
        llama_context_params cparams;
        int myNPredict;
        float myTemp;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized || !model) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Local model is not initialized.");
                }
                return {};
            }
            modelRaw = model.get();
            cparams = llama_context_default_params();
            cparams.n_ctx = static_cast<uint32_t>(std::max(1024, n_ctx));
            cparams.n_batch = std::min<uint32_t>(2048, cparams.n_ctx);
            cparams.n_ubatch = std::min<uint32_t>(256, cparams.n_batch);
            cparams.n_seq_max = 1;
            cparams.n_threads = recommendedInferenceThreads();
            cparams.n_threads_batch = std::max(2, recommendedInferenceThreads() / 2);
            cparams.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_AUTO;
            cparams.offload_kqv = gpuOffloadSupported;
            cparams.no_perf = true;
            cparams.op_offload = gpuOffloadSupported;
            cparams.swa_full = false;
            cparams.kv_unified = false;
            myNPredict = n_predict;
            myTemp = temp;
        }
        return sampleWithModel(modelRaw, cparams, systemPrompt, userPrompt, context, myNPredict, myTemp, &tokenCallback, errorOut);
    }
};

LlamaLocalAgent::LlamaLocalAgent() : impl_(std::make_unique<Impl>()) {}
LlamaLocalAgent::~LlamaLocalAgent() = default;

QString LlamaLocalAgent::lastError() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lastErrorMessage;
}

bool LlamaLocalAgent::initialize(const QString& modelPath) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ensureBackend();
    impl_->lastErrorMessage.clear();

    const QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists() || !modelInfo.isFile()) {
        impl_->lastErrorMessage = QStringLiteral("Model file not found: %1").arg(modelPath);
        qWarning() << "[LlamaLocalAgent]" << impl_->lastErrorMessage;
        impl_->initialized = false;
        return false;
    }

    QString architectureError;
    const QString architecture = readGgufArchitecture(modelPath, &architectureError);
    if (!architectureError.isEmpty()) {
        impl_->lastErrorMessage = architectureError;
        qWarning() << "[LlamaLocalAgent]" << impl_->lastErrorMessage << "path=" << modelPath;
        impl_->initialized = false;
        return false;
    }

    if (!architecture.isEmpty()) {
        qInfo() << "[LlamaLocalAgent] model architecture=" << architecture;
        const QString normalizedArchitecture = architecture.trimmed().toLower();
        if (normalizedArchitecture == QStringLiteral("phi4")) {
            impl_->lastErrorMessage = QStringLiteral(
                "Unsupported GGUF architecture '%1'. This build of llama.cpp supports phi2/phi3/phimoe, but not phi4 yet."
            ).arg(architecture);
            qWarning() << "[LlamaLocalAgent]" << impl_->lastErrorMessage << "path=" << modelPath;
            impl_->initialized = false;
            return false;
        }
    }

    qInfo() << "[LlamaLocalAgent] loading model"
            << "path=" << modelPath
            << "sizeMB=" << (modelInfo.size() / (1024.0 * 1024.0));

    if (impl_->model) {
        impl_->model.reset();
    }

    const size_t availableDevices = llama_max_devices();
    const size_t gpuDevices = countGpuBackends();
    const bool supportsGpuOffload = llama_supports_gpu_offload() && gpuDevices > 0;
    const size_t preferredGpuIndex = supportsGpuOffload ? findPreferredGpuDeviceIndex() : static_cast<size_t>(-1);
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = supportsGpuOffload ? -1 : 0;
    mparams.split_mode = LLAMA_SPLIT_MODE_NONE;
    mparams.main_gpu = (supportsGpuOffload && preferredGpuIndex != static_cast<size_t>(-1))
        ? static_cast<int32_t>(preferredGpuIndex)
        : -1;
    mparams.use_mmap = true;
    mparams.use_mlock = false;
    mparams.check_tensors = false;
    mparams.vocab_only = false;

    impl_->gpuOffloadSupported = supportsGpuOffload;

    qInfo() << "[LlamaLocalAgent] llama device capability"
            << "availableDevices=" << static_cast<qulonglong>(availableDevices)
            << "gpuDevices=" << static_cast<qulonglong>(gpuDevices)
            << "gpuOffloadSupported=" << supportsGpuOffload
            << "preferredGpuIndex=" << (preferredGpuIndex == static_cast<size_t>(-1) ? -1 : static_cast<long long>(preferredGpuIndex))
            << "mainGpu=" << mparams.main_gpu
            << "nGpuLayers=" << mparams.n_gpu_layers;

    const std::string modelPathUtf8 = QStringToUtf8(modelPath);
    llama_model* rawModel = llama_model_load_from_file(modelPathUtf8.c_str(), mparams);
    if (!rawModel) {
        if (!architecture.isEmpty()) {
            impl_->lastErrorMessage = QStringLiteral(
                "llama.cpp failed to load model: %1 (GGUF architecture: %2)"
            ).arg(modelPath, architecture);
        } else {
            impl_->lastErrorMessage = QStringLiteral("llama.cpp failed to load model: %1").arg(modelPath);
        }
        qWarning() << "[LlamaLocalAgent]" << impl_->lastErrorMessage;
        impl_->initialized = false;
        return false;
    }

    impl_->model = LlamaModelPtr(rawModel);
    impl_->modelPath = modelPath;
    impl_->modelName = modelInfo.completeBaseName();
    impl_->initialized = true;

    qInfo() << "[LlamaLocalAgent] model loaded"
            << "path=" << modelPath
            << "name=" << impl_->modelName;
    return true;
}

QString LlamaLocalAgent::analyzeContext(const AIContext& context) {
    auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        return QString("選択中のレイヤー：%1").arg(selectedLayers.front());
    }
    QString activeCompId = context.activeCompositionId();
    if (!activeCompId.isEmpty()) {
        return QString("アクティブコンポジション：%1").arg(activeCompId);
    }
    return "コンテキスト情報なし";
}

QString LlamaLocalAgent::predictParameter(const QString& targetProperty, const AIContext& context) {
    Q_UNUSED(targetProperty);
    Q_UNUSED(context);
    return "{\"value\": 0.5}";
}

bool LlamaLocalAgent::requiresCloudEscalation(const QString& userPrompt, const AIContext& context) {
    Q_UNUSED(context);
    if (userPrompt.contains("script", Qt::CaseInsensitive) ||
        userPrompt.contains("algorithm", Qt::CaseInsensitive) ||
        userPrompt.contains("coding", Qt::CaseInsensitive) ||
        userPrompt.contains("映画", Qt::CaseInsensitive) ||
        userPrompt.contains("cinematic", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

LocalAnalysisResult LlamaLocalAgent::analyzeUserQuestion(const QString& question, const AIContext& context) {
    LocalAnalysisResult result;
    const QString q = question.toLower();

    if (q.contains("見え") || q.contains("表示") || q.contains("visible") ||
        q.contains("display") || q.contains("show")) {
        result.intent = "visibility";
        result.requiredData = {
            "layer.visible",
            "layer.opacity",
            "layer.inPoint",
            "layer.outPoint",
            "layer.solo",
            "layer.parent",
            "layer.locked"
        };
        result.confidence = 0.9f;
    } else if (q.contains("アニメ") || q.contains("動き") || q.contains("motion") ||
               q.contains("animate") || q.contains("keyframe")) {
        result.intent = "animation";
        result.requiredData = {
            "layer.keyframes",
            "layer.properties",
            "layer.inPoint",
            "layer.outPoint"
        };
        result.confidence = 0.85f;
    } else if (q.contains("カラー") || q.contains("色") || q.contains("color") ||
               q.contains("grade") || q.contains("lumetri")) {
        result.intent = "color";
        result.requiredData = {
            "composition.colorSpace",
            "layer.effects",
            "layer.adjustmentLayer"
        };
        result.confidence = 0.85f;
    } else if (q.contains("オーディオ") || q.contains("音声") || q.contains("audio") ||
               q.contains("sound") || q.contains("volume")) {
        result.intent = "audio";
        result.requiredData = {
            "layer.audioLevels",
            "layer.mute",
            "composition.sampleRate"
        };
        result.confidence = 0.85f;
    } else if (q.contains("レンダー") || q.contains("出力") || q.contains("render") ||
               q.contains("export") || q.contains("encode")) {
        result.intent = "render";
        result.requiredData = {
            "composition.resolution",
            "composition.frameRate",
            "renderQueue.status"
        };
        result.confidence = 0.8f;
    } else if (q.contains("エフェクト") || q.contains("effect") ||
               q.contains("filter") || q.contains("plugin")) {
        result.intent = "effect";
        result.requiredData = {
            "layer.effects",
            "effect.parameters",
            "effect.enabled"
        };
        result.confidence = 0.85f;
    } else {
        result.intent = "unknown";
        result.confidence = 0.5f;
    }

    QRegularExpression layerNameRe(R"((?:レイヤー | layer)[「\"']([^「」\"']+)[」\"'])");
    auto match = layerNameRe.match(question);
    if (match.hasMatch()) {
        result.entities["layerName"] = match.captured(1);
    }

    QString collectedData;
    auto selectedLayers = context.selectedLayers();
    if (!selectedLayers.empty()) {
        collectedData += QString("選択中のレイヤー：%1\n").arg(selectedLayers.front());
        for (const auto& dataKey : result.requiredData) {
            if (dataKey == "layer.visible") {
                collectedData += "  表示：(要確認)\n";
            } else if (dataKey == "layer.opacity") {
                collectedData += "  不透明度：(要確認)\n";
            } else if (dataKey == "layer.inPoint") {
                collectedData += "  IN ポイント：(要確認)\n";
            } else if (dataKey == "layer.outPoint") {
                collectedData += "  OUT ポイント：(要確認)\n";
            } else if (dataKey == "layer.solo") {
                collectedData += "  ソロ：(要確認)\n";
            } else if (dataKey == "layer.locked") {
                collectedData += "  ロック：(要確認)\n";
            }
        }
    }

    QString activeCompId = context.activeCompositionId();
    if (!activeCompId.isEmpty()) {
        collectedData += QString("アクティブコンポジション：%1\n").arg(activeCompId);
    }

    result.requiresCloud = false;
    QString error;
    const QString systemPrompt = QStringLiteral("You answer about ArtifactStudio editing tasks.");
    result.localAnswer = impl_->buildAnswer(systemPrompt, question, context, &error);
    if (result.localAnswer.isEmpty()) {
        result.localAnswer = impl_->buildAnswer(
            QStringLiteral("You are a helpful assistant."),
            question,
            context,
            &error);
    }
    if (result.localAnswer.isEmpty()) {
        result.localAnswer = QStringLiteral("申し訳ありません。もう少し詳しく教えていただけますか？");
    }
    result.summarizedContext = filterSensitiveInfo(collectedData);
    return result;
}

QString LlamaLocalAgent::generateChatResponse(
    const QString& systemPrompt,
    const QString& userPrompt,
    const AIContext& context)
{
    QString error;
    const QString answer = impl_->buildAnswer(systemPrompt, userPrompt, context, &error);
    if (answer.isEmpty()) {
        qWarning() << "[LlamaLocalAgent] chat generation failed:" << error;
    }
    return answer;
}

QString LlamaLocalAgent::generateChatResponseStreaming(
    const QString& systemPrompt,
    const QString& userPrompt,
    const AIContext& context,
    const std::function<bool(const QString& piece)>& tokenCallback)
{
    QString error;
    const QString answer = impl_->buildAnswerStreaming(systemPrompt, userPrompt, context, tokenCallback, &error);
    if (answer.isEmpty()) {
        qWarning() << "[LlamaLocalAgent] streaming chat generation failed:" << error;
    }
    return answer;
}

QString LlamaLocalAgent::filterSensitiveInfo(const QString& text) {
    if (text.isEmpty()) return text;

    QString filtered = text;
    filtered.replace(QRegularExpression(R"(C:\\Users\\[^\\]+\\)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"(/Users/[^/]+/)"), "[USER_PATH]/");
    filtered.replace(QRegularExpression(R"([A-Z]:\\[^\\]+\\)"), "[PATH]/");
    filtered.replace(QRegularExpression(R"(\[プロジェクト：[^\]]+\])"), "[プロジェクト]");
    filtered.replace(QRegularExpression(R"(プロジェクト「[^」]+」)"), "プロジェクト");
    filtered.replace(QRegularExpression(R"(user: [^\n]+)"), "user: [ANONYMOUS]");
    return filtered;
}

void LlamaLocalAgent::setMaxTokens(int maxTokens) { impl_->n_predict = maxTokens; }
void LlamaLocalAgent::setTemperature(float temperature) { impl_->temp = temperature; }

} // namespace ArtifactCore
