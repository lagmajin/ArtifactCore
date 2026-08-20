module;
class tst_QList;
#include <utility>
#include <iostream>
#include <QImage>
#include <QColor>
#include <QList>
#include <QRect>
#include <QPainter>
#include <QString>
#include <QStringView>
#include <QVariant>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <cmath>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
#  include <onnxruntime_cxx_api.h>
#  include <onnxruntime_c_api.h>
#  include <onnxruntime/dml_provider_factory.h>
#endif

module Core.AI.ObjectDetector;

import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

class ObjectDetector::Impl {
public:
    float confidence_ = 0.5f;
    QString error_;
    QStringList labels_;
    int inputWidth_ = 640;
    int inputHeight_ = 640;
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string inputName_;
    std::string outputName_;
#endif
    
    // In a real implementation, we'd load a model here (YOLO, Haar Cascade etc.)
    // For now, satisfy the API with OpenCV vision basics.
};

ObjectDetector::ObjectDetector() : impl_(new Impl()) {}

ObjectDetector::~ObjectDetector() {
    delete impl_;
}

bool ObjectDetector::initialize(const QString& modelPath)
{
    impl_->error_.clear();
    if (modelPath.trimmed().isEmpty() || !QFileInfo::exists(modelPath)) {
        impl_->error_ = QStringLiteral("Detection model was not found: %1").arg(modelPath);
        return false;
    }
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    try {
        impl_->env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING,
                                                  "ArtifactObjectDetector");
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(options, 0));
        impl_->session_ = std::make_unique<Ort::Session>(
            *impl_->env_, modelPath.toStdWString().c_str(), options);
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputName = impl_->session_->GetInputNameAllocated(0, allocator);
        auto outputName = impl_->session_->GetOutputNameAllocated(0, allocator);
        impl_->inputName_ = inputName.get();
        impl_->outputName_ = outputName.get();
        const auto inputShape = impl_->session_->GetInputTypeInfo(0)
                                    .GetTensorTypeAndShapeInfo().GetShape();
        if (inputShape.size() >= 4 && inputShape[2] > 0 && inputShape[3] > 0) {
            impl_->inputHeight_ = static_cast<int>(inputShape[2]);
            impl_->inputWidth_ = static_cast<int>(inputShape[3]);
        }
        return true;
    } catch (const Ort::Exception& exception) {
        impl_->error_ = QStringLiteral("ONNX detector initialization failed: %1")
                            .arg(QString::fromUtf8(exception.what()));
        impl_->session_.reset();
        impl_->env_.reset();
        return false;
    }
#else
    impl_->error_ = QStringLiteral("ONNX DirectML backend is not available");
    return false;
#endif
}

bool ObjectDetector::isInitialized() const noexcept
{
#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    return impl_->session_ != nullptr;
#else
    return false;
#endif
}

QString ObjectDetector::lastError() const
{
    return impl_->error_;
}

bool ObjectDetector::setLabelsPath(const QString& path)
{
    QFile file(path.trimmed());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        impl_->error_ = QStringLiteral("Could not open detector labels: %1").arg(path);
        return false;
    }
    QStringList labels;
    QTextStream stream(&file);
    while (!stream.atEnd() && labels.size() < 100000) {
        const QString label = stream.readLine().trimmed();
        if (!label.isEmpty()) labels.append(label);
    }
    impl_->labels_ = std::move(labels);
    impl_->error_.clear();
    return true;
}

QStringList ObjectDetector::classLabels() const
{
    return impl_->labels_;
}

LocalizedText ObjectDetector::briefDescription() const {
    return loc("Detects objects in images using standard vision algorithms.", 
               "標準的な画像認識アルゴリズムを使用して画像内のオブジェクトを検出します。");
}

QList<PropertyDescription> ObjectDetector::propertyDescriptions() const {
    return {
        {"confidenceThreshold", loc("Probability threshold for detections", "検出の確率閾値"), "float", "0.5", "0.0", "1.0"}
    };
}

QList<MethodDescription> ObjectDetector::methodDescriptions() const {
    return {
        {"detect", loc("Detect objects in an image", "画像内のオブジェクトを検出"), "QList<Detection>", {"ImageF32x4_RGBA"}, {"image"}},
        {"detectAndDraw", loc("Detect and draw bounding boxes on the image", "検出して画像に枠を描画"), "void", {"ImageF32x4_RGBA"}, {"image"}}
    };
}

QVariant ObjectDetector::invokeMethod(QStringView name, const QVariantList& args) {
    if (name == "detect" && !args.isEmpty()) {
        // Expected: detect(image)
        // Note: Real implementation would need to handle QVariant to ImageF32x4_RGBA conversion
        return QVariant(); 
    }
    if (name == "detectAndDraw" && !args.isEmpty()) {
        return QVariant();
    }
    if (name == "setConfidenceThreshold" && !args.isEmpty()) {
        setConfidenceThreshold(args[0].toFloat());
        return true;
    }
    return QVariant();
}

// Register for AI discovery (macros are not imported through C++ modules).
static AutoRegisterDescribable<ObjectDetector> _reg_ObjectDetector("ObjectDetector");

QList<Detection> ObjectDetector::detect(const ImageF32x4_RGBA& image) {
    if (image.isEmpty()) return {};

#if defined(ARTIFACT_HAS_ONNX_DML_BACKEND)
    if (impl_->session_) {
        try {
            const int inputWidth = std::clamp(impl_->inputWidth_, 32, 4096);
            const int inputHeight = std::clamp(impl_->inputHeight_, 32, 4096);
            const QImage source = image.toQImage().convertToFormat(QImage::Format_RGBA8888);
            const float resizeScale = std::min(
                static_cast<float>(inputWidth) / std::max(1, source.width()),
                static_cast<float>(inputHeight) / std::max(1, source.height()));
            const int fitWidth = std::max(1, static_cast<int>(std::round(source.width() * resizeScale)));
            const int fitHeight = std::max(1, static_cast<int>(std::round(source.height() * resizeScale)));
            const int padX = (inputWidth - fitWidth) / 2;
            const int padY = (inputHeight - fitHeight) / 2;
            const QImage fitted = source.scaled(fitWidth, fitHeight,
                                                Qt::IgnoreAspectRatio,
                                                Qt::SmoothTransformation);
            QImage resized(inputWidth, inputHeight, QImage::Format_RGBA8888);
            resized.fill(Qt::black);
            for (int y = 0; y < fitHeight; ++y) {
                auto* dst = resized.scanLine(y + padY) + padX * 4;
                const auto* src = fitted.constScanLine(y);
                std::copy_n(src, static_cast<size_t>(fitWidth * 4), dst);
            }
            std::vector<float> input(static_cast<size_t>(3 * inputWidth * inputHeight));
            for (int y = 0; y < inputHeight; ++y) {
                const auto* row = resized.constScanLine(y);
                for (int x = 0; x < inputWidth; ++x) {
                    const int p = x * 4;
                    const size_t offset = static_cast<size_t>(y * inputWidth + x);
                    input[offset] = row[p + 0] / 255.0f;
                    input[static_cast<size_t>(inputWidth * inputHeight) + offset] = row[p + 1] / 255.0f;
                    input[static_cast<size_t>(2 * inputWidth * inputHeight) + offset] = row[p + 2] / 255.0f;
                }
            }
            const std::array<int64_t, 4> shape{1, 3, inputHeight, inputWidth};
            Ort::MemoryInfo memoryInfo("Cpu", OrtArenaAllocator, 0, OrtMemTypeDefault);
            auto tensor = Ort::Value::CreateTensor<float>(memoryInfo, input.data(), input.size(),
                                                           shape.data(), shape.size());
            const char* inputNames[] = {impl_->inputName_.c_str()};
            const char* outputNames[] = {impl_->outputName_.c_str()};
            auto outputs = impl_->session_->Run(Ort::RunOptions{nullptr}, inputNames, &tensor,
                                                1, outputNames, 1);
            if (!outputs.empty() && outputs[0].IsTensor()) {
                const auto typeInfo = outputs[0].GetTensorTypeAndShapeInfo();
                const auto outputShape = typeInfo.GetShape();
                const size_t elementCount = typeInfo.GetElementCount();
                const float* values = outputs[0].GetTensorData<float>();
                int rowCount = 0;
                int fieldCount = 0;
                const bool transposed = outputShape.size() == 3 &&
                    outputShape[1] > 0 && outputShape[2] > 0 &&
                    outputShape[1] < outputShape[2];
                if (outputShape.size() >= 2) {
                    fieldCount = static_cast<int>(transposed ? outputShape[1] : outputShape.back());
                    rowCount = static_cast<int>(elementCount / static_cast<size_t>(std::max(1, fieldCount)));
                }
                if (values && fieldCount >= 6 && rowCount > 0) {
                    QList<Detection> results;
                    const auto valueAt = [&](int row, int field) {
                        if (transposed) {
                            return values[static_cast<size_t>(field * rowCount + row)];
                        }
                        return values[static_cast<size_t>(row * fieldCount + field)];
                    };
                    for (int row = 0; row < rowCount; ++row) {
                        const bool hasObjectness = !transposed && fieldCount > 6;
                        float score = hasObjectness ? valueAt(row, 4) : 0.0f;
                        int classOffset = 5;
                        const int classStart = transposed ? 4 : 5;
                        if (transposed || hasObjectness) {
                            float bestClass = 0.0f;
                            int bestIndex = 0;
                            for (int c = classStart; c < fieldCount; ++c) {
                                if (valueAt(row, c) > bestClass) {
                                    bestClass = valueAt(row, c);
                                    bestIndex = c - classStart;
                                }
                            }
                            score = hasObjectness ? score * bestClass : bestClass;
                            classOffset = bestIndex;
                        }
                        if (score < impl_->confidence_) continue;
                        const bool xywh = transposed || hasObjectness;
                        float x1 = valueAt(row, 0);
                        float y1 = valueAt(row, 1);
                        float x2 = valueAt(row, 2);
                        float y2 = valueAt(row, 3);
                        if (xywh) {
                            x1 -= x2 * 0.5f;
                            y1 -= y2 * 0.5f;
                            x2 += x1;
                            y2 += y1;
                        }
                        const auto toImageX = [&](float value) {
                            return static_cast<int>(std::clamp((value - padX) / resizeScale,
                                                               0.0f, static_cast<float>(image.width())));
                        };
                        const auto toImageY = [&](float value) {
                            return static_cast<int>(std::clamp((value - padY) / resizeScale,
                                                               0.0f, static_cast<float>(image.height())));
                        };
                        Detection result;
                        result.label = classOffset >= 0 && classOffset < impl_->labels_.size()
                            ? impl_->labels_.at(classOffset)
                            : QStringLiteral("class_%1").arg(classOffset);
                        result.confidence = score;
                        result.rect = QRect(QPoint(toImageX(x1), toImageY(y1)),
                                            QPoint(toImageX(x2), toImageY(y2))).normalized();
                        results.append(result);
                    }
                    std::sort(results.begin(), results.end(),
                              [](const Detection& left, const Detection& right) {
                                  return left.confidence > right.confidence;
                              });
                    QList<Detection> filtered;
                    constexpr float kNmsIoU = 0.45f;
                    for (const Detection& candidate : results) {
                        bool suppressed = false;
                        for (const Detection& kept : filtered) {
                            if (candidate.label != kept.label) continue;
                            const QRect intersection = candidate.rect.intersected(kept.rect);
                            const int intersectionArea = intersection.isEmpty() ? 0 : intersection.width() * intersection.height();
                            const int unionArea = candidate.rect.width() * candidate.rect.height() +
                                kept.rect.width() * kept.rect.height() - intersectionArea;
                            if (unionArea > 0 && static_cast<float>(intersectionArea) / unionArea > kNmsIoU) {
                                suppressed = true;
                                break;
                            }
                        }
                        if (!suppressed) filtered.append(candidate);
                    }
                    return filtered;
                }
            }
        } catch (const Ort::Exception& exception) {
            impl_->error_ = QStringLiteral("ONNX detector inference failed: %1")
                                .arg(QString::fromUtf8(exception.what()));
        }
    }
#endif

    QImage qimage = image.toQImage();
    if (qimage.isNull()) return {};
    QImage gray = qimage.convertToFormat(QImage::Format_Grayscale8);

    QList<Detection> results;

    struct BrightestPixel {
        int x = 0;
        int value = -1;
    };
    std::vector<BrightestPixel> rowBest(static_cast<size_t>(gray.height()));
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *row = gray.constScanLine(y);
        auto& brightest = rowBest[static_cast<size_t>(y)];
        for (int x = 0; x < gray.width(); ++x) {
            const int value = row[x];
            if (value > brightest.value) {
                brightest.value = value;
                brightest.x = x;
            }
        }
    }

    int bestX = 0;
    int bestY = 0;
    int bestValue = -1;
    for (int y = 0; y < gray.height(); ++y) {
        const auto& brightest = rowBest[static_cast<size_t>(y)];
        if (brightest.value > bestValue) {
            bestValue = brightest.value;
            bestX = brightest.x;
            bestY = y;
        }
    }

    if (bestValue > static_cast<int>(0.8f * 255.0f)) {
        Detection d;
        d.label = "Subject";
        d.confidence = static_cast<float>(bestValue) / 255.0f;
        d.rect = QRect(bestX - 25, bestY - 25, 50, 50);
        results.append(d);
    }
    
    return results;
}

void ObjectDetector::detectAndDraw(ImageF32x4_RGBA& image) {
    auto detections = detect(image);
    if (detections.isEmpty()) return;

    QImage qimage = image.toQImage().convertToFormat(QImage::Format_RGBA8888);
    if (qimage.isNull()) return;
    QPainter painter(&qimage);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const auto& d : detections) {
        painter.setPen(QPen(QColor(0, 255, 0), 2));
        painter.drawRect(d.rect);
    }

    image.setFromRGBA8(qimage.bits(), qimage.width(), qimage.height());
}

void ObjectDetector::setConfidenceThreshold(float threshold) {
    impl_->confidence_ = threshold;
}

float ObjectDetector::confidenceThreshold() const {
    return impl_->confidence_;
}

} // namespace ArtifactCore
