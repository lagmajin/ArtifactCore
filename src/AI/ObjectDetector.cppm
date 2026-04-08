module;
#include <utility>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringView>
#include <QVariant>

module Core.AI.ObjectDetector;

import Image.ImageF32x4_RGBA;
import CvUtils;

namespace ArtifactCore {

class ObjectDetector::Impl {
public:
    float confidence_ = 0.5f;
    
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
    if (image.isEmpty()) return {};

    cv::Mat mat = image.toCVMat();
    cv::Mat gray = CvUtils::ensureGray(mat);
    
    QList<Detection> results;
    
    // (Simulate) Real detection would be: 
    // cv::CascadeClassifier classifier; classifier.detectMultiScale(gray, objects);
    // For this stub, let's detect the "brightest" spot as an object
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(gray, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal > 0.8) { // Assuming float 0.0-1.0
        Detection d;
        d.label = "Subject";
        d.confidence = static_cast<float>(maxVal);
        d.rect = QRect(maxLoc.x - 25, maxLoc.y - 25, 50, 50);
        results.append(d);
    }
    
    return results;
}

void ObjectDetector::detectAndDraw(ImageF32x4_RGBA& image) {
    auto detections = detect(image);
    if (detections.isEmpty()) return;

    cv::Mat mat = image.toCVMat();
    
    for (const auto& d : detections) {
        cv::Rect rect(d.rect.x(), d.rect.y(), d.rect.width(), d.rect.height());
        CvUtils::drawDetection(mat, rect, d.label.toStdString(), cv::Scalar(0, 1, 0, 1)); // Green RGBA
    }
}

void ObjectDetector::setConfidenceThreshold(float threshold) {
    impl_->confidence_ = threshold;
}

float ObjectDetector::confidenceThreshold() const {
    return impl_->confidence_;
}

} // namespace ArtifactCore
