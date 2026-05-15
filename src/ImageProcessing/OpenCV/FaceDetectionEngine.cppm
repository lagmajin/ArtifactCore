module;
class tst_QList;
#include <utility>
#include <memory>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QRect>
#include <QVector>
#include <QString>
#include <QStringView>
#include <QStringList>
#include <QFileInfo>

module ArtifactCore.ImageProcessing.FaceDetection;

import CvUtils;

namespace ArtifactCore {

class FaceDetectionEngine::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    FaceDetectionSettings settings_;
    cv::CascadeClassifier haarCascade_;
    cv::dnn::Net dnnNet_;
    bool haarLoaded_ = false;
    bool dnnLoaded_ = false;
    QString lastError_;
    QString haarCascadePath_;
    QString dnnModelPath_;
    QString dnnConfigPath_;

    bool initializeHaarCascade() {
        if (haarCascadePath_.isEmpty()) {
            // デフォルトパスを試す
            QStringList candidates = {
                QStringLiteral("haarcascade_frontalface_default.xml"),
                QStringLiteral("C:/opencv/sources/data/haarcascades/haarcascade_frontalface_default.xml"),
            };
            for (const auto& path : candidates) {
                if (QFileInfo::exists(path)) {
                    haarCascadePath_ = path;
                    break;
                }
            }
        }

        if (haarCascadePath_.isEmpty()) {
            lastError_ = QStringLiteral("Haar cascade file not found. Set path with setHaarCascadePath().");
            return false;
        }

        haarLoaded_ = haarCascade_.load(haarCascadePath_.toStdString());
        if (!haarLoaded_) {
            lastError_ = QStringLiteral("Failed to load Haar cascade: %1").arg(QStringView{haarCascadePath_});
        }
        return haarLoaded_;
    }

    bool initializeDNN() {
        if (dnnModelPath_.isEmpty() || dnnConfigPath_.isEmpty()) {
            lastError_ = QStringLiteral("DNN model/config paths not set. Use setDNNModelPath() and setDNNConfigPath().");
            return false;
        }

        try {
            dnnNet_ = cv::dnn::readNetFromTensorflow(dnnModelPath_.toStdString(), dnnConfigPath_.toStdString());
            if (dnnNet_.empty()) {
                dnnNet_ = cv::dnn::readNetFromCaffe(dnnConfigPath_.toStdString(), dnnModelPath_.toStdString());
            }
            dnnLoaded_ = !dnnNet_.empty();
        } catch (const cv::Exception& e) {
            lastError_ = QStringLiteral("DNN load error: %1").arg(QStringView{QString::fromUtf8(e.what())});
            dnnLoaded_ = false;
        }

        if (!dnnLoaded_) {
            lastError_ = QStringLiteral("Failed to load DNN model: %1 / %2").arg(QStringView{dnnModelPath_}, QStringView{dnnConfigPath_});
        }
        return dnnLoaded_;
    }

    QVector<FaceDetectionResult> detectHaar(const cv::Mat& image) {
        QVector<FaceDetectionResult> results;
        if (!haarLoaded_ || image.empty()) return results;

        cv::Mat gray;
        if (image.channels() == 3) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
        } else {
            gray = image;
        }

        cv::equalizeHist(gray, gray);

        cv::Size minSize(settings_.minSize.width(), settings_.minSize.height());
        cv::Size maxSize;
        if (settings_.maxSize.width() > 0 && settings_.maxSize.height() > 0) {
            maxSize = cv::Size(settings_.maxSize.width(), settings_.maxSize.height());
        }

        std::vector<cv::Rect> faces;
        haarCascade_.detectMultiScale(
            gray,
            faces,
            settings_.scaleFactor,
            settings_.minNeighbors,
            0,
            minSize,
            maxSize
        );

        results.reserve(static_cast<int>(faces.size()));
        for (const auto& face : faces) {
            FaceDetectionResult result;
            result.rect = QRect(face.x, face.y, face.width, face.height);
            result.confidence = 0.8f; // Haar Cascade は信頼度を返さない
            results.append(result);
        }

        return results;
    }

    QVector<FaceDetectionResult> detectDNN(const cv::Mat& image) {
        QVector<FaceDetectionResult> results;
        if (!dnnLoaded_ || image.empty() || dnnNet_.empty()) return results;

        cv::Mat blob;
        cv::Mat inputImage = image;
        if (inputImage.channels() == 4) {
            cv::cvtColor(inputImage, inputImage, cv::COLOR_BGRA2BGR);
        }

        cv::dnn::blobFromImage(
            inputImage,
            blob,
            1.0,
            cv::Size(settings_.dnnWidth, settings_.dnnHeight),
            cv::Scalar(104.0, 177.0, 123.0),
            false,
            false
        );

        dnnNet_.setInput(blob);
        cv::Mat detections = dnnNet_.forward();

        float imgWidth = static_cast<float>(image.cols);
        float imgHeight = static_cast<float>(image.rows);

        for (int i = 0; i < detections.size[2]; ++i) {
            float confidence = detections.ptr<float>(0, 0, i)[2];
            if (confidence < 0.5f) continue;

            const float* row = detections.ptr<float>(0, 0, i);
            int x1 = static_cast<int>(row[3] * imgWidth);
            int y1 = static_cast<int>(row[4] * imgHeight);
            int x2 = static_cast<int>(row[5] * imgWidth);
            int y2 = static_cast<int>(row[6] * imgHeight);

            FaceDetectionResult result;
            result.rect = QRect(x1, y1, x2 - x1, y2 - y1);
            result.confidence = confidence;
            results.append(result);
        }

        return results;
    }

    QVector<FaceDetectionResult> detectMat(const cv::Mat& image) {
        if (image.empty()) return {};

        bool useDNN = (settings_.method == FaceDetectionMethod::DNN) ||
                      (settings_.method == FaceDetectionMethod::Auto && dnnLoaded_);

        if (useDNN && dnnLoaded_) {
            return detectDNN(image);
        } else if (haarLoaded_) {
            return detectHaar(image);
        }

        return {};
    }
};

FaceDetectionEngine::FaceDetectionEngine() : impl_(std::make_unique<Impl>()) {}

FaceDetectionEngine::~FaceDetectionEngine() = default;

bool FaceDetectionEngine::initialize(const FaceDetectionSettings& settings) {
    impl_->settings_ = settings;

    bool haarOk = impl_->initializeHaarCascade();
    bool dnnOk = impl_->initializeDNN();

    return haarOk || dnnOk;
}

bool FaceDetectionEngine::isInitialized() const {
    return impl_->haarLoaded_ || impl_->dnnLoaded_;
}

void FaceDetectionEngine::setSettings(const FaceDetectionSettings& settings) {
    impl_->settings_ = settings;
}

const FaceDetectionSettings& FaceDetectionEngine::settings() const {
    return impl_->settings_;
}

QVector<FaceDetectionResult> FaceDetectionEngine::detect(const QImage& image) {
    if (image.isNull()) return {};
    cv::Mat mat = CvUtils::qImageToCvMat(image, true);
    return detect(mat);
}

QVector<FaceDetectionResult> FaceDetectionEngine::detect(const cv::Mat& image) {
    return impl_->detectMat(image);
}

void FaceDetectionEngine::setHaarCascadePath(const QString& path) {
    impl_->haarCascadePath_ = path;
    impl_->haarLoaded_ = false;
    impl_->initializeHaarCascade();
}

void FaceDetectionEngine::setDNNModelPath(const QString& modelPath) {
    impl_->dnnModelPath_ = modelPath;
    impl_->dnnLoaded_ = false;
    impl_->initializeDNN();
}

void FaceDetectionEngine::setDNNConfigPath(const QString& configPath) {
    impl_->dnnConfigPath_ = configPath;
    impl_->dnnLoaded_ = false;
    impl_->initializeDNN();
}

QString FaceDetectionEngine::lastError() const {
    return impl_->lastError_;
}

} // namespace ArtifactCore
