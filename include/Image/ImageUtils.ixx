module;
#include <OpenImageIO/imageio.h>
#include <QImage>
export module Image.Utils;

import Image.ExportOptions;
import Image.ImageF32x4RGBAWithCache;

export namespace ArtifactCore {
 using namespace OIIO;

 // Utility function to create a shared_ptr of ImageF32x4RGBAWithCache
 // Use ImageF32x4RGBAWithCachePtr typedef for cleaner code
 inline ImageF32x4RGBAWithCachePtr createImageCache() {
  return std::make_shared<ImageF32x4RGBAWithCache>();
 }
 
 inline ImageF32x4RGBAWithCachePtr createImageCache(const ImageF32x4_RGBA& image) {
  return std::make_shared<ImageF32x4RGBAWithCache>(image);
 }
 
 inline ImageF32x4RGBAWithCachePtr deepCopyImageCache(const ImageF32x4RGBAWithCachePtr& src) {
  if (!src) return nullptr;
  return std::make_shared<ImageF32x4RGBAWithCache>(src->DeepCopy());
 }

 ImageSpec createSpec(const QImage& image, const ImageExportOptions& opt) {
  // 1. 解像度とチャンネル数の設定 (QImageは基本4ch)
  // チャンネル数はQImageのフォーマットに応じて動的に変えても良い
  int nchannels = 4;
  ImageSpec spec(image.width(), image.height(), nchannels, TypeDesc::UINT8);

  // 2. データ型の設定 (Optionsから反映)
  // ※ 内部でTypeDesc::HALFなどが指定されている想定
  // spec.set_format(opt.dataType); 

  // 3. 圧縮設定
  if (!opt.format.isEmpty()) {
   spec.attribute("compression", opt.compression.toStdString());
   if (opt.compressionQuality > 0) {
	spec.attribute("CompressionQuality", (int)opt.compressionQuality);
   }
  }

  // 4. タイル設定 (0より大きい場合のみ)
  if (opt.tileWidth > 0 && opt.tileHeight > 0) {
   spec.tile_width = opt.tileWidth;
   spec.tile_height = opt.tileHeight;
  }

  // 5. メタデータ (ColorSpace, Creator等)
  spec.set_colorspace(opt.colorSpace.toStdString());
  if (!opt.creator.isEmpty()) spec.attribute("Artist", opt.creator.toStdString());
  if (!opt.copyright.isEmpty()) spec.attribute("Copyright", opt.copyright.toStdString());

  return spec;
 }






};