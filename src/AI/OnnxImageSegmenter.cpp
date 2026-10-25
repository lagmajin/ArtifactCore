module;
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
#  include <onnxruntime_cxx_api.h>
#  include <onnxruntime_c_api.h>
#  include <onnxruntime/dml_provider_factory.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

module Core.AI.OnnxImageSegmenter;

import Core.AI.ImageSegmenter;

namespace ArtifactCore {

#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)

namespace {

float sampleChannel(const ImageF32x4_RGBA& image, int channel, float x, float y)
{
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, image.width() - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, image.height() - 1);
    const int x1 = std::min(x0 + 1, image.width() - 1);
    const int y1 = std::min(y0 + 1, image.height() - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto a = image.getPixel(x0, y0);
    const auto b = image.getPixel(x1, y0);
    const auto c = image.getPixel(x0, y1);
    const auto d = image.getPixel(x1, y1);
    const auto component = [channel](const auto& pixel) {
        switch (channel) {
        case 0: return pixel.r();
        case 1: return pixel.g();
        case 2: return pixel.b();
        default: return pixel.a();
        }
    };
    const float top = component(a) + (component(b) - component(a)) * tx;
    const float bottom = component(c) + (component(d) - component(c)) * tx;
    return top + (bottom - top) * ty;
}

float sigmoid(float value)
{
    return value >= 0.0f
        ? 1.0f / (1.0f + std::exp(-value))
        : std::exp(value) / (1.0f + std::exp(value));
}

float sampleMaskBilinear(const float* values, size_t channelOffset,
                         int width, int height, float normalizedX, float normalizedY)
{
    const float x = std::clamp(normalizedX, 0.0f, 1.0f) * static_cast<float>(width - 1);
    const float y = std::clamp(normalizedY, 0.0f, 1.0f) * static_cast<float>(height - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto sample = [values, channelOffset, width](int sampleX, int sampleY) {
        return values[channelOffset + static_cast<size_t>(sampleY) * width + sampleX];
    };
    const float top = sample(x0, y0) + (sample(x1, y0) - sample(x0, y0)) * tx;
    const float bottom = sample(x0, y1) + (sample(x1, y1) - sample(x0, y1)) * tx;
    return top + (bottom - top) * ty;
}

float normalizeInput(float value, int channel, const OnnxImageSegmentationOptions& options)
{
    if (channel == 3) { return value; }
    const int colorChannel = options.inputColorOrder == OnnxImageInputColorOrder::BGR
        ? (channel == 0 ? 2 : (channel == 2 ? 0 : channel)) : channel;
    const float mean = colorChannel == 0 ? options.inputRedMean :
        (colorChannel == 1 ? options.inputGreenMean : options.inputBlueMean);
    const float stdDev = colorChannel == 0 ? options.inputRedStdDev :
        (colorChannel == 1 ? options.inputGreenStdDev : options.inputBlueStdDev);
    return (value * options.inputScale - mean) / std::max(stdDev, 0.000001f);
}

int sourceChannelForModelInput(int channel, const OnnxImageSegmentationOptions& options)
{
    if (channel == 3 || options.inputColorOrder != OnnxImageInputColorOrder::BGR) {
        return channel;
    }
    return channel == 0 ? 2 : (channel == 2 ? 0 : channel);
}

float activateOutput(const float* values, size_t foregroundOffset, size_t outputPlaneSize,
                     int channelCount, float normalizedX, float normalizedY,
                     int width, int height, const OnnxImageSegmentationOptions& options)
{
    const float foreground = sampleMaskBilinear(
        values, foregroundOffset, width, height, normalizedX, normalizedY);
    if (options.outputActivation == OnnxSegmentationOutputActivation::None) {
        return foreground;
    }
    if (options.outputActivation == OnnxSegmentationOutputActivation::Sigmoid || channelCount == 1) {
        return sigmoid(foreground);
    }
    float maxLogit = foreground;
    for (int channel = 0; channel < channelCount; ++channel) {
        maxLogit = std::max(maxLogit, sampleMaskBilinear(
            values, static_cast<size_t>(channel) * outputPlaneSize,
            width, height, normalizedX, normalizedY));
    }
    float denominator = 0.0f;
    for (int channel = 0; channel < channelCount; ++channel) {
        denominator += std::exp(sampleMaskBilinear(
            values, static_cast<size_t>(channel) * outputPlaneSize,
            width, height, normalizedX, normalizedY) - maxLogit);
    }
    return denominator > 0.0f ? std::exp(foreground - maxLogit) / denominator : 0.0f;
}

struct InputGeometry {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

InputGeometry inputGeometry(int sourceWidth, int sourceHeight, int inputWidth, int inputHeight,
                            bool preserveAspectRatio)
{
    if (!preserveAspectRatio) {
        return {static_cast<float>(inputWidth) / sourceWidth,
                static_cast<float>(inputHeight) / sourceHeight, 0.0f, 0.0f};
    }
    const float scale = std::min(static_cast<float>(inputWidth) / sourceWidth,
                                 static_cast<float>(inputHeight) / sourceHeight);
    return {scale, scale,
            (inputWidth - sourceWidth * scale) * 0.5f,
            (inputHeight - sourceHeight * scale) * 0.5f};
}

bool mapsIntoSource(float inputCoordinate, float offset, float scale, int sourceSize)
{
    const float sourceCoordinate = (inputCoordinate + 0.5f - offset) / scale - 0.5f;
    return sourceCoordinate >= -0.5f && sourceCoordinate <= static_cast<float>(sourceSize) - 0.5f;
}

} // namespace

class OnnxImageSegmenter::Impl {
public:
    OnnxImageSegmentationOptions options;
    QString lastErrorMessage;
    std::unique_ptr<Ort::Env> environment;
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::string outputName;
    int inputWidth = 0;
    int inputHeight = 0;
    int inputChannels = 3;
    bool usingDirectML = false;
};

OnnxImageSegmenter::OnnxImageSegmenter() : impl_(new Impl()) {}

OnnxImageSegmenter::~OnnxImageSegmenter()
{
    delete impl_;
    impl_ = nullptr;
}

bool OnnxImageSegmenter::initialize(const QString& modelPath)
{
    reset();
    impl_->lastErrorMessage.clear();
    if (!QFileInfo::exists(modelPath)) {
        impl_->lastErrorMessage = QStringLiteral("Segmentation model not found: %1").arg(modelPath);
        return false;
    }
    try {
        impl_->environment = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ArtifactOnnxImageSegmenter");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (impl_->options.preferDirectML) {
            try {
                Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));
                impl_->usingDirectML = true;
            } catch (const Ort::Exception&) {
                // Keep the model usable on systems without a DirectML-capable
                // adapter. modelInfo() exposes the selected backend.
                impl_->usingDirectML = false;
            }
        }
        const std::wstring nativePath = modelPath.toStdWString();
        impl_->session = std::make_unique<Ort::Session>(*impl_->environment, nativePath.c_str(), sessionOptions);
        if (impl_->session->GetInputCount() == 0 || impl_->session->GetOutputCount() == 0) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model must expose an input and output tensor.");
            reset();
            return false;
        }
        if (impl_->options.outputIndex < 0 ||
            static_cast<size_t>(impl_->options.outputIndex) >= impl_->session->GetOutputCount()) {
            impl_->lastErrorMessage = QStringLiteral("Configured segmentation output index is unavailable.");
            reset();
            return false;
        }
        Ort::AllocatorWithDefaultOptions allocator;
        impl_->inputName = impl_->session->GetInputNameAllocated(0, allocator).get();
        impl_->outputName = impl_->session->GetOutputNameAllocated(
            static_cast<size_t>(impl_->options.outputIndex), allocator).get();
        const auto shape = impl_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() != 4) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model input must be an NCHW tensor.");
            reset();
            return false;
        }
        impl_->inputChannels = shape[1] > 0 ? static_cast<int>(shape[1]) : 3;
        impl_->inputHeight = impl_->options.inputHeight > 0 ? impl_->options.inputHeight :
            (shape[2] > 0 ? static_cast<int>(shape[2]) : 512);
        impl_->inputWidth = impl_->options.inputWidth > 0 ? impl_->options.inputWidth :
            (shape[3] > 0 ? static_cast<int>(shape[3]) : 512);
        if (impl_->inputChannels < 1 || impl_->inputChannels > 4 ||
            impl_->inputWidth < 1 || impl_->inputHeight < 1) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model has unsupported input dimensions.");
            reset();
            return false;
        }
        return true;
    } catch (const Ort::Exception& error) {
        impl_->lastErrorMessage = QStringLiteral("ONNX segmentation initialization failed: %1")
            .arg(QString::fromUtf8(error.what()));
    }
    reset();
    return false;
}

bool OnnxImageSegmenter::loadOptionsFromJson(const QString& configurationPath)
{
    if (!impl_) { return false; }
    QFile file(configurationPath);
    if (!file.open(QIODevice::ReadOnly)) {
        impl_->lastErrorMessage = QStringLiteral("Segmentation configuration could not be opened: %1")
            .arg(configurationPath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        impl_->lastErrorMessage = QStringLiteral("Segmentation configuration must be a JSON object: %1")
            .arg(parseError.errorString());
        return false;
    }
    const QJsonObject object = document.object();
    OnnxImageSegmentationOptions options = impl_->options;
    options.inputWidth = object.value(QStringLiteral("inputWidth")).toInt(options.inputWidth);
    options.inputHeight = object.value(QStringLiteral("inputHeight")).toInt(options.inputHeight);
    options.outputIndex = object.value(QStringLiteral("outputIndex")).toInt(options.outputIndex);
    options.foregroundChannel = object.value(QStringLiteral("foregroundChannel")).toInt(options.foregroundChannel);
    options.preserveAspectRatio = object.value(QStringLiteral("preserveAspectRatio"))
        .toBool(options.preserveAspectRatio);
    options.inputPaddingValue = static_cast<float>(object.value(QStringLiteral("inputPaddingValue"))
        .toDouble(options.inputPaddingValue));
    options.inputScale = static_cast<float>(object.value(QStringLiteral("inputScale"))
        .toDouble(options.inputScale));
    options.inputRedMean = static_cast<float>(object.value(QStringLiteral("inputRedMean"))
        .toDouble(options.inputRedMean));
    options.inputGreenMean = static_cast<float>(object.value(QStringLiteral("inputGreenMean"))
        .toDouble(options.inputGreenMean));
    options.inputBlueMean = static_cast<float>(object.value(QStringLiteral("inputBlueMean"))
        .toDouble(options.inputBlueMean));
    options.inputRedStdDev = static_cast<float>(object.value(QStringLiteral("inputRedStdDev"))
        .toDouble(options.inputRedStdDev));
    options.inputGreenStdDev = static_cast<float>(object.value(QStringLiteral("inputGreenStdDev"))
        .toDouble(options.inputGreenStdDev));
    options.inputBlueStdDev = static_cast<float>(object.value(QStringLiteral("inputBlueStdDev"))
        .toDouble(options.inputBlueStdDev));
    options.preferDirectML = object.value(QStringLiteral("preferDirectML"))
        .toBool(options.preferDirectML);
    const QString colorOrder = object.value(QStringLiteral("inputColorOrder"))
        .toString().trimmed().toLower();
    if (colorOrder == QStringLiteral("rgb")) {
        options.inputColorOrder = OnnxImageInputColorOrder::RGB;
    } else if (colorOrder == QStringLiteral("bgr")) {
        options.inputColorOrder = OnnxImageInputColorOrder::BGR;
    } else if (!colorOrder.isEmpty()) {
        impl_->lastErrorMessage = QStringLiteral("Segmentation configuration inputColorOrder must be rgb or bgr.");
        return false;
    }
    const QString activation = object.value(QStringLiteral("outputActivation"))
        .toString().trimmed().toLower();
    if (activation == QStringLiteral("none")) {
        options.outputActivation = OnnxSegmentationOutputActivation::None;
    } else if (activation == QStringLiteral("softmax")) {
        options.outputActivation = OnnxSegmentationOutputActivation::Softmax;
    } else if (!activation.isEmpty() && activation != QStringLiteral("sigmoid")) {
        impl_->lastErrorMessage = QStringLiteral("Unsupported segmentation outputActivation: %1").arg(activation);
        return false;
    } else if (activation == QStringLiteral("sigmoid")) {
        options.outputActivation = OnnxSegmentationOutputActivation::Sigmoid;
    }
    if (options.inputWidth < 0 || options.inputHeight < 0 || options.outputIndex < 0 ||
        options.foregroundChannel < 0 || options.inputRedStdDev <= 0.0f ||
        options.inputGreenStdDev <= 0.0f || options.inputBlueStdDev <= 0.0f) {
        impl_->lastErrorMessage = QStringLiteral("Segmentation configuration has invalid dimensions, channels, or standard deviations.");
        return false;
    }
    reset();
    impl_->options = options;
    impl_->lastErrorMessage.clear();
    return true;
}

void OnnxImageSegmenter::reset() noexcept
{
    impl_->session.reset();
    impl_->environment.reset();
    impl_->inputName.clear();
    impl_->outputName.clear();
    impl_->inputWidth = 0;
    impl_->inputHeight = 0;
    impl_->usingDirectML = false;
}

bool OnnxImageSegmenter::isReady() const noexcept
{
    return impl_ && impl_->session != nullptr;
}

bool OnnxImageSegmenter::segment(const ImageF32x4_RGBA& source, DepthMap& foregroundMask)
{
    if (!isReady() || source.isEmpty()) {
        foregroundMask.clear();
        return false;
    }
    try {
        const size_t planeSize = static_cast<size_t>(impl_->inputWidth) * impl_->inputHeight;
        std::vector<float> input(planeSize * static_cast<size_t>(impl_->inputChannels));
        const InputGeometry geometry = inputGeometry(
            source.width(), source.height(), impl_->inputWidth, impl_->inputHeight,
            impl_->options.preserveAspectRatio);
        for (int y = 0; y < impl_->inputHeight; ++y) {
            const float sourceY = (static_cast<float>(y) + 0.5f - geometry.offsetY) /
                geometry.scaleY - 0.5f;
            const bool yInSource = mapsIntoSource(static_cast<float>(y), geometry.offsetY,
                                                   geometry.scaleY, source.height());
            for (int x = 0; x < impl_->inputWidth; ++x) {
                const float sourceX = (static_cast<float>(x) + 0.5f - geometry.offsetX) /
                    geometry.scaleX - 0.5f;
                const bool inSource = yInSource && mapsIntoSource(
                    static_cast<float>(x), geometry.offsetX, geometry.scaleX, source.width());
                const size_t offset = static_cast<size_t>(y) * impl_->inputWidth + x;
                for (int channel = 0; channel < impl_->inputChannels; ++channel) {
                    const float value = inSource
                        ? sampleChannel(source, sourceChannelForModelInput(
                            channel, impl_->options), sourceX, sourceY)
                        : impl_->options.inputPaddingValue;
                    input[static_cast<size_t>(channel) * planeSize + offset] =
                        normalizeInput(value, channel, impl_->options);
                }
            }
        }
        const std::array<int64_t, 4> shape{1, impl_->inputChannels, impl_->inputHeight, impl_->inputWidth};
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape.data(), shape.size());
        const char* inputName = impl_->inputName.c_str();
        const char* outputName = impl_->outputName.c_str();
        const auto output = impl_->session->Run(Ort::RunOptions{nullptr}, &inputName, &tensor, 1, &outputName, 1);
        if (output.empty() || !output.front().IsTensor()) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model returned no tensor output.");
            return false;
        }
        const auto outputInfo = output.front().GetTensorTypeAndShapeInfo();
        const auto outputShape = outputInfo.GetShape();
        if (outputInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || outputShape.size() < 2) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model must return a float mask tensor.");
            return false;
        }
        const int outputWidth = static_cast<int>(outputShape.back());
        const int outputHeight = static_cast<int>(outputShape[outputShape.size() - 2]);
        const int outputChannels = outputShape.size() >= 3 && outputShape[outputShape.size() - 3] > 0
            ? static_cast<int>(outputShape[outputShape.size() - 3]) : 1;
        if (outputWidth < 1 || outputHeight < 1 || impl_->options.foregroundChannel < 0 ||
            impl_->options.foregroundChannel >= outputChannels) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model output shape or foreground channel is unsupported.");
            return false;
        }
        const float* values = output.front().GetTensorData<float>();
        if (!values) {
            impl_->lastErrorMessage = QStringLiteral("Segmentation model returned an empty mask.");
            return false;
        }
        foregroundMask.resize(source.width(), source.height());
        const size_t outputPlaneSize = static_cast<size_t>(outputWidth) * outputHeight;
        const size_t channelOffset = static_cast<size_t>(impl_->options.foregroundChannel) * outputPlaneSize;
        for (int y = 0; y < source.height(); ++y) {
            const float inputY = (static_cast<float>(y) + 0.5f) * geometry.scaleY +
                geometry.offsetY - 0.5f;
            const float normalizedY = impl_->inputHeight > 1
                ? inputY / static_cast<float>(impl_->inputHeight - 1) : 0.0f;
            for (int x = 0; x < source.width(); ++x) {
                const float inputX = (static_cast<float>(x) + 0.5f) * geometry.scaleX +
                    geometry.offsetX - 0.5f;
                const float normalizedX = impl_->inputWidth > 1
                    ? inputX / static_cast<float>(impl_->inputWidth - 1) : 0.0f;
                foregroundMask.setValue(x, y, activateOutput(
                    values, channelOffset, outputPlaneSize, outputChannels,
                    normalizedX, normalizedY, outputWidth, outputHeight, impl_->options));
            }
        }
        impl_->lastErrorMessage.clear();
        return true;
    } catch (const Ort::Exception& error) {
        impl_->lastErrorMessage = QStringLiteral("ONNX segmentation inference failed: %1")
            .arg(QString::fromUtf8(error.what()));
    }
    foregroundMask.clear();
    return false;
}

void OnnxImageSegmenter::setOptions(const OnnxImageSegmentationOptions& options) noexcept
{
    impl_->options = options;
}

OnnxImageSegmentationOptions OnnxImageSegmenter::options() const noexcept { return impl_->options; }
OnnxImageSegmentationModelInfo OnnxImageSegmenter::modelInfo() const
{
    OnnxImageSegmentationModelInfo info;
    if (!impl_) { return info; }
    info.ready = impl_->session != nullptr;
    info.usingDirectML = impl_->usingDirectML;
    info.inputWidth = impl_->inputWidth;
    info.inputHeight = impl_->inputHeight;
    info.inputChannels = impl_->inputChannels;
    info.inputName = QString::fromStdString(impl_->inputName);
    info.outputName = QString::fromStdString(impl_->outputName);
    return info;
}
QString OnnxImageSegmenter::lastError() const { return impl_ ? impl_->lastErrorMessage : QString(); }

#else

class OnnxImageSegmenter::Impl {
public:
    OnnxImageSegmentationOptions options;
    QString lastErrorMessage = QStringLiteral("ONNX Runtime with DirectML is unavailable in this build.");
};

OnnxImageSegmenter::OnnxImageSegmenter() : impl_(new Impl()) {}
OnnxImageSegmenter::~OnnxImageSegmenter() { delete impl_; impl_ = nullptr; }
bool OnnxImageSegmenter::initialize(const QString&) { return false; }
bool OnnxImageSegmenter::loadOptionsFromJson(const QString&)
{
    impl_->lastErrorMessage = QStringLiteral("ONNX Runtime with DirectML is unavailable in this build.");
    return false;
}
void OnnxImageSegmenter::reset() noexcept {}
bool OnnxImageSegmenter::isReady() const noexcept { return false; }
bool OnnxImageSegmenter::segment(const ImageF32x4_RGBA&, DepthMap& mask) { mask.clear(); return false; }
void OnnxImageSegmenter::setOptions(const OnnxImageSegmentationOptions& options) noexcept { impl_->options = options; }
OnnxImageSegmentationOptions OnnxImageSegmenter::options() const noexcept { return impl_->options; }
OnnxImageSegmentationModelInfo OnnxImageSegmenter::modelInfo() const { return {}; }
QString OnnxImageSegmenter::lastError() const { return impl_ ? impl_->lastErrorMessage : QString(); }

#endif

} // namespace ArtifactCore
