module;
#include <utility>
#include <QString>
#include <QStringView>
#include <QVariant>
#include <QList>
#include <QRect>
#include <QImage>

export module Core.AI.ObjectDetector;

import Image.ImageF32x4_RGBA;
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

/**
 * @brief Implementation of Object Detection AI
 */
class ObjectDetector : public IDescribable {
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
    QList<Detection> detect(const ImageF32x4_RGBA& image);

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
