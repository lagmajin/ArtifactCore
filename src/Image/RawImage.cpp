module;

module Image.Raw;
import std;

namespace ArtifactCore {


 int RawImage::getPixelTypeSizeInBytes() const
 {
  if (pixelType == "uint8") return 1;
  if (pixelType == "int8") return 1;
  if (pixelType == "uint16") return 2;
  if (pixelType == "int16") return 2;
  if (pixelType == "half") return 2;
  if (pixelType == "uint32") return 4;
  if (pixelType == "int32") return 4;
  if (pixelType == "float") return 4;
  if (pixelType == "uint64") return 8;
  if (pixelType == "int64") return 8;
  if (pixelType == "double") return 8;

  // 未知のタイプの場合
  return 0;
 }

 bool RawImage::isValid() const
 {
  return width > 0 && height > 0 && channels > 0 && !data.isEmpty() && !pixelType.isEmpty();
 }



};