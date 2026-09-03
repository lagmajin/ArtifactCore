module;
export module Core.AI.ImageSegmenter;
import Image.DepthMap;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {
inline void applySegmentationMask(ImageF32x4_RGBA&, const DepthMap&, float) noexcept {}
}
