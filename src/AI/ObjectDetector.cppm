module;
class tst_QList;
#include <utility>
#include <iostream>
#include <algorithm>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringView>
#include <QVariant>

module Core.AI.ObjectDetector;

import Image.ImageF32x4_RGBA;
import FloatRGBA;

namespace ArtifactCore {

class ObjectDetector::Impl {
public:
    float confidence_ = 0.5f;
    QString lastErrorMessage;
    
    // In a real implementation, we'd load a model here (YOLO, Haar Cascade etc.)
    // For now, satisfy the API with OpenCV vision basics.
};

ObjectDetector::ObjectDetector() : impl_(new Impl()) {}

ObjectDetector::~ObjectDetector() {
    delete impl_;
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
    if (image.isEmpty()) {
        impl_->lastErrorMessage = QStringLiteral("Object detection source image is empty.");
        return {};
    }

    QList<Detection> results;
    int bestX = 0;
    int bestY = 0;
    float bestLuminance = -1.0f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            const float luminance = std::clamp(
                pixel.r() * 0.2126f + pixel.g() * 0.7152f + pixel.b() * 0.0722f,
                0.0f, 1.0f);
            if (luminance > bestLuminance) {
                bestLuminance = luminance;
                bestX = x;
                bestY = y;
            }
        }
    }

    if (bestLuminance >= impl_->confidence_) {
        Detection d;
        d.label = "Subject";
        d.confidence = bestLuminance;
        d.rect = QRect(bestX - 25, bestY - 25, 50, 50)
            .intersected(QRect(0, 0, image.width(), image.height()));
        results.append(d);
    }
    impl_->lastErrorMessage.clear();
    
    return results;
}

bool ObjectDetector::isReady() const noexcept { return impl_ != nullptr; }

QString ObjectDetector::lastError() const {
    return impl_ ? impl_->lastErrorMessage : QString();
}

void ObjectDetector::detectAndDraw(ImageF32x4_RGBA& image) {
    auto detections = detect(image);
    if (detections.isEmpty()) return;
    for (const auto& d : detections) {
        const QRect rect = d.rect.intersected(QRect(0, 0, image.width(), image.height()));
        for (int thickness = 0; thickness < 2; ++thickness) {
            const int left = rect.left() + thickness;
            const int right = rect.right() - thickness;
            const int top = rect.top() + thickness;
            const int bottom = rect.bottom() - thickness;
            for (int x = left; x <= right; ++x) {
                image.setPixel(x, top, FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
                image.setPixel(x, bottom, FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
            }
            for (int y = top; y <= bottom; ++y) {
                image.setPixel(left, y, FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
                image.setPixel(right, y, FloatRGBA(0.0f, 1.0f, 0.0f, 1.0f));
            }
        }
    }
}

bool rasterizeDetectionMask(const QList<Detection>& detections,
                            int width, int height,
                            DepthMap& mask,
                            float minimumConfidence,
                            int featherPixels) {
    if (width <= 0 || height <= 0) { mask.clear(); return false; }
    mask.resize(width, height);
    const float threshold = std::clamp(minimumConfidence, 0.0f, 1.0f);
    const int feather = std::max(featherPixels, 0);
    const QRect canvas(0, 0, width, height);
    for (const auto& detection : detections) {
        if (detection.confidence < threshold) { continue; }
        const QRect rect = detection.rect.intersected(canvas);
        if (rect.isEmpty()) { continue; }
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                float coverage = 1.0f;
                if (feather > 0) {
                    const int edgeDistance = std::min(
                        std::min(x - rect.left(), rect.right() - x),
                        std::min(y - rect.top(), rect.bottom() - y));
                    coverage = std::clamp(
                        static_cast<float>(edgeDistance + 1) / static_cast<float>(feather + 1),
                        0.0f, 1.0f);
                }
                mask.setValue(x, y, std::max(mask.value(x, y), coverage));
            }
        }
    }
    return true;
}

void ObjectDetector::setConfidenceThreshold(float threshold) {
    impl_->confidence_ = std::clamp(threshold, 0.0f, 1.0f);
}

float ObjectDetector::confidenceThreshold() const {
    return impl_->confidence_;
}

} // namespace ArtifactCore
