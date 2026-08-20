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

module Core.AI.OnnxImageSegmenter;

import Core.AI.ImageSegmenter;

namespace ArtifactCore {

class OnnxImageSegmenter::Impl {
public:
    QString error;
    int inputWidth = 512;
    int inputHeight = 512;
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::string outputName;
#endif
};

OnnxImageSegmenter::OnnxImageSegmenter() : impl_(new Impl()) {}
OnnxImageSegmenter::~OnnxImageSegmenter() { delete impl_; }

bool OnnxImageSegmenter::initialize(const QString& modelPath)
{
    impl_->error.clear();
    if (modelPath.trimmed().isEmpty() || !QFileInfo::exists(modelPath)) {
        impl_->error = QStringLiteral("Segmentation model was not found: %1").arg(modelPath);
        return false;
    }
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    try {
        impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                                 "ArtifactImageSegmenter");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));
        impl_->session = std::make_unique<Ort::Session>(
            *impl_->env, modelPath.toStdWString().c_str(), sessionOptions);
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
        impl_->error = QStringLiteral("ONNX segmentation initialization failed: %1")
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

bool OnnxImageSegmenter::isInitialized() const noexcept
{
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    return impl_->session != nullptr;
#else
    return false;
#endif
}

DepthMap OnnxImageSegmenter::segment(const ImageF32x4_RGBA& image,
                                     const ImageSegmentationOptions& options)
{
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    if (!impl_->session || image.isEmpty()) return {};
    try {
        const int modelWidth = std::clamp(impl_->inputWidth, 32, 4096);
        const int modelHeight = std::clamp(impl_->inputHeight, 32, 4096);
        const QImage source = image.toQImage().convertToFormat(QImage::Format_RGBA8888);
        const QImage resized = source.scaled(modelWidth, modelHeight,
                                             Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation);
        std::vector<float> input(static_cast<size_t>(3 * modelWidth * modelHeight));
        for (int y = 0; y < modelHeight; ++y) {
            const auto* row = resized.constScanLine(y);
            for (int x = 0; x < modelWidth; ++x) {
                const int p = x * 4;
                const size_t offset = static_cast<size_t>(y * modelWidth + x);
                input[offset] = row[p + 0] / 255.0f;
                input[static_cast<size_t>(modelWidth * modelHeight) + offset] = row[p + 1] / 255.0f;
                input[static_cast<size_t>(2 * modelWidth * modelHeight) + offset] = row[p + 2] / 255.0f;
            }
        }
        const std::array<int64_t, 4> shape{1, 3, modelHeight, modelWidth};
        Ort::MemoryInfo memoryInfo("Cpu", OrtArenaAllocator, 0, OrtMemTypeDefault);
        auto tensor = Ort::Value::CreateTensor<float>(memoryInfo, input.data(), input.size(),
                                                       shape.data(), shape.size());
        const char* inputNames[] = {impl_->inputName.c_str()};
        const char* outputNames[] = {impl_->outputName.c_str()};
        auto outputs = impl_->session->Run(Ort::RunOptions{nullptr}, inputNames, &tensor,
                                           1, outputNames, 1);
        if (outputs.empty() || !outputs[0].IsTensor()) return {};
        const auto typeInfo = outputs[0].GetTensorTypeAndShapeInfo();
        const auto outputShape = typeInfo.GetShape();
        const size_t elementCount = typeInfo.GetElementCount();
        const float* values = outputs[0].GetTensorData<float>();
        if (!values || elementCount == 0) return {};
        const int height = outputShape.size() >= 2
            ? static_cast<int>(outputShape[outputShape.size() - 2]) : modelHeight;
        const int width = outputShape.size() >= 1
            ? static_cast<int>(outputShape.back()) : modelWidth;
        if (width <= 0 || height <= 0 || static_cast<size_t>(width) * height > elementCount) {
            return {};
        }
        DepthMap result(width, height);
        std::copy_n(values, static_cast<size_t>(width) * height, result.values().begin());
        if (options.applySigmoid) {
            for (float& value : result.values()) {
                value = 1.0f / (1.0f + std::exp(-value));
            }
        }
        if (options.normalize) result.normalize();
        if (options.invert) result.invert();
        if (options.threshold > 0.0f && options.threshold < 1.0f) {
            for (float& value : result.values()) value = value >= options.threshold ? 1.0f : 0.0f;
        }
        return result;
    } catch (const Ort::Exception& exception) {
        impl_->error = QStringLiteral("ONNX segmentation inference failed: %1")
                           .arg(QString::fromUtf8(exception.what()));
        return {};
    }
#else
    Q_UNUSED(image);
    Q_UNUSED(options);
    return {};
#endif
}

QString OnnxImageSegmenter::backendName() const
{
    return QStringLiteral("onnx-directml-segmentation");
}

QString OnnxImageSegmenter::lastError() const
{
    return impl_->error;
}

}
