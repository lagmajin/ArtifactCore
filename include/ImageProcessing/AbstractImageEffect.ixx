module;

export module ImageProcessing;
export import :AffineTransform;
import ImageF32x4;



export namespace ArtifactCore {



class AbstractImageEffect {
private:

public:
 AbstractImageEffect();
 virtual ~AbstractImageEffect() = default;
 //virtual void process(ImageF32x4_RGBA& image) = 0;

};






};