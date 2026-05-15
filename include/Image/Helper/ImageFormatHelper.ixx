module;
#include <utility>
#include <OpenImageIO/typedesc.h>
#include "../../Define/DllExportMacro.hpp"
export module Image.Format.Helper;

#include <QImage>

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
  case QImage::Format_BGR888:     // OIIO���Ń`�����l�������̍l�����K�v�����^��UINT8
   return { TypeDesc::UINT8, 3 };
  default:
   // �݊������Ȃ��ꍇ�� RGBA8888 �ɕϊ����Ĉ����O��Őݒ�
   return { TypeDesc::UINT8, 4 };
  }
 }

};
