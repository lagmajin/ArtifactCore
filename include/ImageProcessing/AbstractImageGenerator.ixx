module;
#include <utility>
#include <memory>

#include <QString>

export module Generator:Image;

import Image.ImageF32x4_RGBA;
import Size;

export namespace ArtifactCore {

/**
 * @brief Base class for any component that generates image data procedurally.
 */
class AbstractImageGenerator {
public:
    virtual ~AbstractImageGenerator() = default;

    /**
     * @brief Generate image into the provided buffer
     */
    virtual bool generate(ImageF32x4_RGBA& output) = 0;

    /**
     * @brief Create a new image with generated content
     */
    virtual std::unique_ptr<ImageF32x4_RGBA> create(int width, int height) {
        auto img = std::make_unique<ImageF32x4_RGBA>();
        img->resize(width, height);
        if (generate(*img)) {
            return img;
        }
        return nullptr;
    }

    /**
     * @brief Name of this generator for UI/AI
     */
    virtual QString name() const = 0;
};

} // namespace ArtifactCore
