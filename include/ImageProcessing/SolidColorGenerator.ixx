module;
#include <QString>

export module Generator:SolidColor;

import Generator:Image;
import Image.ImageF32x4_RGBA;
import FloatRGBA;

export namespace ArtifactCore {

/**
 * @brief Simple generator that fills the buffer with a solid color.
 */
class SolidColorGenerator : public AbstractImageGenerator {
public:
    SolidColorGenerator(const FloatRGBA& color) : color_(color) {}

    bool generate(ImageF32x4_RGBA& output) override {
        output.fill(color_);
        return true;
    }

    QString name() const override { return "Solid Color"; }

    void setColor(const FloatRGBA& color) { color_ = color; }
    FloatRGBA color() const { return color_; }

private:
    FloatRGBA color_;
};

} // namespace ArtifactCore
