module;
class tst_QList;
#include <algorithm>
#include <utility>
#include <QString>
#include <QStringView>
#include <QVariant>
#include <QList>
#include <QRect>

export module Core.AI.ObjectDetector;

import Image.ImageF32x4_RGBA;
import Image.DepthMap;
import Core.AI.Describable;

export namespace ArtifactCore {

/**
 * @brief Represents a single detected object
 */
struct Detection {
    QString label;
    float confidence;
    QRect rect;
};

class IObjectDetector {
public:
    virtual ~IObjectDetector() = default;
    virtual bool isReady() const noexcept = 0;
    virtual QList<Detection> detect(const ImageF32x4_RGBA& image) = 0;
    virtual QString lastError() const = 0;
};

// Converts selected detection rectangles into a normalized foreground mask so
// detection, AI segmentation, and manual mask tools share one matte format.
bool rasterizeDetectionMask(const QList<Detection>& detections,
                            int width, int height,
                            DepthMap& mask,
                            float minimumConfidence = 0.0f,
                            int featherPixels = 0);

inline QList<Detection> filterDetections(
    const QList<Detection>& detections, QStringView label = {},
    float minimumConfidence = 0.0f) {
    QList<Detection> filtered;
    const float threshold = std::clamp(minimumConfidence, 0.0f, 1.0f);
    for (const auto& detection : detections) {
        if (detection.confidence < threshold ||
            (!label.isEmpty() && detection.label != label)) {
            continue;
        }
        filtered.append(detection);
    }
    return filtered;
}

/**
 * @brief Implementation of Object Detection AI
 */
class ObjectDetector : public IDescribable, public IObjectDetector {
public:
    ObjectDetector();
    ~ObjectDetector();

    // IDescribable overrides
    QString className() const override { return "ObjectDetector"; }
    LocalizedText briefDescription() const override;
    QList<PropertyDescription> propertyDescriptions() const override;
    QList<MethodDescription> methodDescriptions() const override;
    QVariant invokeMethod(QStringView name, const QVariantList& args) override;

    /**
     * @brief Detect objects in an image
     */
    bool isReady() const noexcept override;
    QList<Detection> detect(const ImageF32x4_RGBA& image) override;
    QString lastError() const override;

    /**
     * @brief Detect and draw bounding boxes on the image
     */
    void detectAndDraw(ImageF32x4_RGBA& image);

    // Settings
    void setConfidenceThreshold(float threshold);
    float confidenceThreshold() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
