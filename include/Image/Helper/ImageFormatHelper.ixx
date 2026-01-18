module;
#include <OpenImageIO/typedesc.h>
#include <QImage>
#include "../../Define/DllExportMacro.hpp"
export module Image.Format.Helper;

export namespace ArtifactCore {
 struct ImageFormatInfo {
  OIIO::TypeDesc type;
  int channels;
 };

 LIBRARY_DLL_API ImageFormatInfo mapQtFormatToOIIO(QImage::Format format) {
  using namespace OIIO;
  switch (format) {
  case QImage::Format_RGB888:     return { TypeDesc::UINT8, 3 };
  case QImage::Format_RGBA8888:   return { TypeDesc::UINT8, 4 };
  case QImage::Format_Grayscale8: return { TypeDesc::UINT8, 1 };
  case QImage::Format_RGBA64:     return { TypeDesc::UINT16, 4 };
  case QImage::Format_Grayscale16:return { TypeDesc::UINT16, 1 };
  case QImage::Format_BGR888:     // OIIO側でチャンネル順序の考慮が必要だが型はUINT8
   return { TypeDesc::UINT8, 3 };
  default:
   // 互換性がない場合は RGBA8888 に変換して扱う前提で設定
   return { TypeDesc::UINT8, 4 };
  }
 }

};