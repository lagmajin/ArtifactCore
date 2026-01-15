module;
#define DISABLE_TIFF
#include <QImage>
#include <OpenImageIo/imageio.h>
#include <OpenImageIO/filesystem.h>

#include <QByteArray>
#include <wobjectimpl.h>

module IO.ImageExporter;


namespace ArtifactCore
{
 using namespace OIIO;

W_OBJECT_IMPL(ImageExporter)

 class ImageExporter::Impl
 {
 public:
  Impl();
  //~Impl();
 };

 ImageExporter::Impl::Impl()
 {

 }

 ImageExporter::ImageExporter(QObject* parent/*=nullptr*/) :QObject(parent), impl_(new Impl)
 {

 }


 ImageExporter::~ImageExporter()
 {
  delete impl_;
 }

 bool ImageExporter::write()
 {


  return true;
 }

 ImageExportResult ImageExporter::testWrite()
 {
  ImageExportResult result;

  int w = 512, h = 512;
  QImage qimg(w, h, QImage::Format_RGBA8888);
  for (int y = 0; y < h; ++y) {
   for (int x = 0; x < w; ++x) {
	// XとYでグラデーション
	qimg.setPixelColor(x, y, QColor(x % 256, y % 256, 150));
   }
  }
  std::vector<unsigned char> myBuffer;

  Filesystem::IOVecOutput buffer_proxy(myBuffer);

  const char* name = "test.png";
  OIIO::string_view sv(name, 8);
  // "png"フォーマットを指定してOutput作成
  auto out = ImageOutput::create(sv, &buffer_proxy);
  if (!out) {
   result.error_message = geterror();
   return result;
  }

  // チャンネル数4(RGBA), UINT8
  ImageSpec spec(w, h, 4, TypeDesc::UINT8);
  //spec.attribute("oiio:ioproxy", TypeDesc::PTR, &buffer_proxy);
  // --- 5. 書き込み実行 ---
  // 第一引数はダミー名だが、拡張子でエンコーダーが決まる

  const std::wstring filename = L"dummy.png";
  if (!out->open(filename, spec)) {
   result.error_message = out->geterror();
   return result;
  }

  // QImageの生のポインタを渡す
  if (out->write_image(TypeDesc::UINT8, qimg.constBits())) {
   result.success = true;
  }
  else {
   result.error_message = out->geterror();
  }

  out->close();

  if (!myBuffer.empty()) {
   std::string filename = "test_output.png";
   std::ofstream ofs(filename, std::ios::binary);
   if (ofs) {
	ofs.write(reinterpret_cast<const char*>(myBuffer.data()), myBuffer.size());
	std::cout << "Success! File exported: " << filename << " (" << myBuffer.size() << " bytes)" << std::endl;
   }
  }

  return result;
 }

 ImageExportResult ImageExporter::testWrite2()
 {
  ImageExportResult result;
  int w = 512, h = 512;

  // 1. 画像生成（グラデーション）
  QImage qimg(w, h, QImage::Format_RGBA8888);
  for (int y = 0; y < h; ++y) {
   for (int x = 0; x < w; ++x) {
	qimg.setPixelColor(x, y, QColor(x % 256, y % 256, 150));
   }
  }

  // 2. Outputの作成（プロキシは渡さない）
  // 拡張子 .png から自動的にエンコーダーが選ばれます
  auto out = ImageOutput::create("test_direct.tif");
  if (!out) {
   result.error_message = geterror();
   return result;
  }

  // 3. スペック設定
  ImageSpec spec(w, h, 4, TypeDesc::UINT8);

  // 4. ファイルオープン（ここでもプロキシ関連の属性は不要）
  // MSVCでエラーが出る場合は L"test_direct.png" にしてください
  if (!out->open("test_direct.png", spec)) {
   result.error_message = out->geterror();
   return result;
  }

  // 5. 書き込み実行
  if (out->write_image(TypeDesc::UINT8, qimg.constBits())) {
   result.success = true;
   std::cout << "Successfully wrote directly to file." << std::endl;
  }
  else {
   result.error_message = out->geterror();
  }

  out->close();
  return result;
 }

 ImageExporterSubmitResult ImageExporter::writeAsync(const QImage& image, const ImageExportOptions& options)
 {
  ImageExporterSubmitResult result;

  ImageSpec spec(image.width(), image.height(), 4, TypeDesc::UINT8);



  return result;
 }

};