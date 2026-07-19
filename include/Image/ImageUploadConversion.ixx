export module Image.UploadConversion;

import Image.ImageF32x4_RGBA;
export import Image.SurfacePixelConversion;

export namespace ArtifactCore {

using ImageUploadTarget = SurfacePixelTarget;
using ImageUploadBuffer = SurfacePixelBuffer;

inline ImageUploadBuffer convertImageForUpload(
    const ImageF32x4_RGBA &image, ImageUploadTarget target) {
  return convertSurfacePixels(image.rgba32fData(), image.rgba8Data(),
                              image.width(), image.height(),
                              image.colorDescriptor(), target);
}

} // namespace ArtifactCore
