module;
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <QString>
#include <QFileInfo>
#include <QImage>

#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
#  include <onnxruntime_cxx_api.h>
#  include <onnxruntime_c_api.h>
#  include <onnxruntime/dml_provider_factory.h>
#endif

module Core.AI.OnnxImageDepthEstimator;

import Core.AI.ImageDepthEstimator;

namespace ArtifactCore {

class OnnxImageDepthEstimator::Impl {
public:
    QString modelPath;
    QString error;
    int inputWidth = 384;
    int inputHeight = 384;
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::string outputName;
#endif
};

OnnxImageDepthEstimator::OnnxImageDepthEstimator() : impl_(new Impl()) {}
OnnxImageDepthEstimator::~OnnxImageDepthEstimator() { delete impl_; }

bool OnnxImageDepthEstimator::initialize(const QString& modelPath)
{
    impl_->error.clear();
    impl_->modelPath = modelPath.trimmed();
    if (impl_->modelPath.isEmpty() || !QFileInfo::exists(impl_->modelPath)) {
        impl_->error = QStringLiteral("Depth model was not found: %1").arg(impl_->modelPath);
        return false;
    }
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    try {
        impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                                 "ArtifactDepthEstimator");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(
            sessionOptions, 0));
        impl_->session = std::make_unique<Ort::Session>(
            *impl_->env, impl_->modelPath.toStdWString().c_str(), sessionOptions);
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = impl_->session->GetInputNameAllocated(0, allocator);
        auto outputName = impl_->session->GetOutputNameAllocated(0, allocator);
        impl_->inputName = inputName.get();
        impl_->outputName = outputName.get();
        const auto inputShape = impl_->session->GetInputTypeInfo(0)
                                    .GetTensorTypeAndShapeInfo().GetShape();
        if (inputShape.size() >= 4 && inputShape[2] > 0 && inputShape[3] > 0) {
            impl_->inputHeight = static_cast<int>(inputShape[2]);
            impl_->inputWidth = static_cast<int>(inputShape[3]);
        }
        return true;
    } catch (const Ort::Exception& exception) {
        impl_->error = QStringLiteral("ONNX depth model initialization failed: %1")
                           .arg(QString::fromUtf8(exception.what()));
        impl_->session.reset();
        impl_->env.reset();
        return false;
    }
#else
    impl_->error = QStringLiteral("ONNX DirectML backend is not available");
    return false;
#endif
}

bool OnnxImageDepthEstimator::isInitialized() const noexcept
{
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    return impl_->session != nullptr;
#else
    return false;
#endif
}

DepthMap OnnxImageDepthEstimator::estimate(
    const ImageF32x4_RGBA& image, const ImageDepthEstimateOptions& options)
{
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    if (!impl_->session || image.isEmpty()) return {};
    try {
        const QImage source = image.toQImage().convertToFormat(QImage::Format_RGBA8888);
        const int modelWidth = std::clamp(impl_->inputWidth, 32, 4096);
        const int modelHeight = std::clamp(impl_->inputHeight, 32, 4096);
        const QImage resized = source.scaled(modelWidth, modelHeight,
                                             Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation);
        std::vector<float> input(static_cast<size_t>(3 * modelWidth * modelHeight));
        for (int y = 0; y < modelHeight; ++y) {
            const auto* row = resized.constScanLine(y);
            for (int x = 0; x < modelWidth; ++x) {
                const int pixel = x * 4;
                const size_t offset = static_cast<size_t>(y * modelWidth + x);
                input[offset] = static_cast<float>(row[pixel + 0]) / 255.0f;
                input[static_cast<size_t>(modelWidth * modelHeight) + offset] =
                    static_cast<float>(row[pixel + 1]) / 255.0f;
                input[static_cast<size_t>(2 * modelWidth * modelHeight) + offset] =
                    static_cast<float>(row[pixel + 2]) / 255.0f;
            }
        }
        const std::array<int64_t, 4> shape{1, 3, modelHeight, modelWidth};
        Ort::MemoryInfo memoryInfo("Cpu", OrtArenaAllocator, 0, OrtMemTypeDefault);
        Ort::Value tensor = Ort::Value::CreateTensor<float>(
            memoryInfo, input.data(), input.size(), shape.data(), shape.size());
        const char* inputNames[] = {impl_->inputName.c_str()};
        const char* outputNames[] = {impl_->outputName.c_str()};
        auto outputs = impl_->session->Run(Ort::RunOptions{nullptr}, inputNames,
                                           &tensor, 1, outputNames, 1);
        if (outputs.empty() || !outputs[0].IsTensor()) return {};
        auto info = outputs[0].GetTensorTypeAndShapeInfo();
        const auto outputShape = info.GetShape();
        const size_t elementCount = info.GetElementCount();
        const float* output = outputs[0].GetTensorData<float>();
        if (!output || elementCount == 0) return {};
        const int outputHeight = outputShape.size() >= 2
            ? static_cast<int>(outputShape[outputShape.size() - 2]) : modelHeight;
        const int outputWidth = outputShape.size() >= 1
            ? static_cast<int>(outputShape.back()) : modelWidth;
        if (outputWidth <= 0 || outputHeight <= 0 ||
            static_cast<size_t>(outputWidth) * static_cast<size_t>(outputHeight) > elementCount) {
            return {};
        }
        DepthMap result(outputWidth, outputHeight);
        std::copy_n(output, static_cast<size_t>(outputWidth) * outputHeight,
                    result.values().begin());
        if (options.normalize) result.normalize();
        if (options.invert) result.invert();
        return result;
    } catch (const Ort::Exception& exception) {
        impl_->error = QStringLiteral("ONNX depth inference failed: %1")
                           .arg(QString::fromUtf8(exception.what()));
        return {};
    }
#else
    Q_UNUSED(image);
    Q_UNUSED(options);
    return {};
#endif
}

QString OnnxImageDepthEstimator::backendName() const
{
    return QStringLiteral("onnx-directml");
}

QString OnnxImageDepthEstimator::lastError() const
{
    return impl_->error;
}

}
