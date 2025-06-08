module;

export module ColorTransform;
export import :ColorTransformCV;


import Image;

export namespace ArtifactCore {


 void RGBAF32x4toHSV(ImageF32x4_RGBA image);
 void RGAAF32x4toYUV420(ImageF32x4_RGBA image);

}