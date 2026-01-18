module;
//#include <OpenImageIO/>

#include <QDir>
#include <QObject>
#include <QFuture>
#include <wobjectdefs.h>

#include "../../Define/DllExportMacro.hpp"
export module IO.ImageExporter;

import Image;
import Image.Utils;
import Image.ExportOptions;

//namespace OIIO {};//dummy

export namespace ArtifactCore {

 //using namespace OIIO;

 struct ImageExportResult {
  bool success = false;
  //std::vector<unsigned char> data; // メモリ書き出し時のバイナリ
  std::string error_message;      // OIIOからのエラー詳細
  size_t byte_size = 0;           // 書き出されたサイズ

  // 成功か失敗かをサクッと判定するためのヘルパー
  explicit operator bool() const { return success; }
 };

struct ImageExporterSubmitResult {
 enum class Status {
  Accepted,
  Rejected,
  ImmediateError
 };
  
 std::future<ImageExportResult>  future;
  
 Status status;
 operator bool() const { return status == Status::Accepted; }
};

 class LIBRARY_DLL_API ImageExporter :public QObject
 {
  W_OBJECT(ImageExporter)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ImageExporter(QObject* parent = nullptr);
  ~ImageExporter();
  ImageExporterSubmitResult writeAsync(const QImage& image,const ImageExportOptions& options);

  ImageExportResult testWrite();
  ImageExportResult testWrite2();
   
  

 public/*signals*/:
 };





}