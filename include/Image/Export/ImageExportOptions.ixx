module;

#include <QString>
#include <OpenImageIO/imageio.h> // OIIO
#include <OpenImageIO/typedesc.h> // O
export module Image:ImageExports;




export namespace ArtifactCore {

 struct ImageExportOptions {
  // ファイルフォーマット (OIIO文字列)
  QString format;
  // 圧縮方式 (OIIO文字列)
  QString compression;
  // 圧縮品質 (0.0f - 100.0f)
  float compressionQuality;
  // データ型 (OIIO TypeDesc)
  //TypeDesc dataType;
  // 色空間 (OIIO文字列)
  QString colorSpace;
  // タイル幅
  int tileWidth;
  // タイル高さ
  int tileHeight;
  // 作成者メタデータ
  QString creator;
  // コピーライトメタデータ
  QString copyright;

  // デフォルト値
  ImageExportOptions() :
   format("exr"),
   compression("zip"),
   compressionQuality(90.0f),
   //dataType(TypeDesc::HALF), // デフォルトはプロダクションで一般的なHalf
   colorSpace("acescg"),
   tileWidth(0), // 0はタイル化しない
   tileHeight(0),
   creator(""),
   copyright("")
  {
  }
 };

}