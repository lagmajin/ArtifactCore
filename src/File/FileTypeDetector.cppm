module;
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>

module File.TypeDetector;

namespace ArtifactCore {

class FileTypeDetector::Impl {
public:
  Impl();
  ~Impl();

  FileType detectByExtension(const QString &filePath) const;
  FileType detectByMagicNumber(const QString &filePath) const;
};

FileTypeDetector::Impl::Impl() {}

FileTypeDetector::Impl::~Impl() {}

FileType
FileTypeDetector::Impl::detectByExtension(const QString &filePath) const {
  QFileInfo fileInfo(filePath);
  QString suffix = fileInfo.suffix().toLower();

  // Image
  if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" ||
      suffix == "bmp" || suffix == "gif" || suffix == "tiff" ||
      suffix == "tif" || suffix == "tga" || suffix == "exr" ||
      suffix == "hdr" || suffix == "psd" || suffix == "psb" ||
      suffix == "webp" || suffix == "svg" || suffix == "ico" ||
      suffix == "dds" || suffix == "ktx") {
    return FileType::Image;
  }
  // Video
  if (suffix == "mp4" || suffix == "avi" || suffix == "mov" ||
      suffix == "mkv" || suffix == "wmv" || suffix == "flv" ||
      suffix == "webm" || suffix == "m4v" || suffix == "mpeg" ||
      suffix == "mpg" || suffix == "3gp" || suffix == "ts") {
    return FileType::Video;
  }
  // Audio
  if (suffix == "mp3" || suffix == "wav" || suffix == "flac" ||
      suffix == "aac" || suffix == "ogg" || suffix == "wma" ||
      suffix == "m4a" || suffix == "aiff" || suffix == "opus" ||
      suffix == "ac3" || suffix == "dts") {
    return FileType::Audio;
  }
  // Font
  if (suffix == "ttf" || suffix == "otf" || suffix == "woff" ||
      suffix == "woff2" || suffix == "eot") {
    return FileType::Document; // Font maps to Document for now
  }
  // Text
  if (suffix == "txt" || suffix == "md" || suffix == "json" ||
      suffix == "xml" || suffix == "html" || suffix == "css" ||
      suffix == "js" || suffix == "yaml" || suffix == "yml" ||
      suffix == "toml" || suffix == "ini" || suffix == "cfg" ||
      suffix == "log" || suffix == "csv") {
    return FileType::Text;
  }
  // Document
  if (suffix == "pdf" || suffix == "doc" || suffix == "docx" ||
      suffix == "xls" || suffix == "xlsx" || suffix == "ppt" ||
      suffix == "pptx") {
    return FileType::Document;
  }
  // Archive
  if (suffix == "zip" || suffix == "rar" || suffix == "7z" || suffix == "tar" ||
      suffix == "gz" || suffix == "bz2" || suffix == "xz") {
    return FileType::Archive;
  }
  // 3D Model
  if (suffix == "obj" || suffix == "fbx" || suffix == "gltf" ||
      suffix == "glb" || suffix == "stl" || suffix == "blend" ||
      suffix == "dae" || suffix == "abc" || suffix == "usd" ||
      suffix == "usdz" || suffix == "pmd" || suffix == "pmx") {
    return FileType::Model3D;
  }

  return FileType::Unknown;
}

FileType
FileTypeDetector::Impl::detectByMagicNumber(const QString &filePath) const {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return FileType::Unknown;
  }

  QByteArray header = file.read(16); // Read first 16 bytes
  file.close();

  if (header.size() < 4) {
    return FileType::Unknown;
  }

  // PNG
  if (header.startsWith("\x89PNG")) {
    return FileType::Image;
  }
  // Photoshop PSD/PSB
  if (header.startsWith("8BPS")) {
    return FileType::Image;
  }
  // JPEG
  if (header.startsWith("\xFF\xD8\xFF")) {
    return FileType::Image;
  }
  // GIF
  if (header.startsWith("GIF8")) {
    return FileType::Image;
  }
  // BMP
  if (header.startsWith("BM")) {
    return FileType::Image;
  }
  // MP4
  if (header.mid(4, 4) == "ftyp") {
    return FileType::Video;
  }
  // AVI
  if (header.startsWith("RIFF") && header.mid(8, 4) == "AVI ") {
    return FileType::Video;
  }
  // MP3
  if (header.startsWith("ID3") || header.startsWith("\xFF\xFB") ||
      header.startsWith("\xFF\xF3") || header.startsWith("\xFF\xF2")) {
    return FileType::Audio;
  }
  // WAV
  if (header.startsWith("RIFF") && header.mid(8, 4) == "WAVE") {
    return FileType::Audio;
  }
  // ZIP
  if (header.startsWith("PK\x03\x04")) {
    return FileType::Archive;
  }
  // RAR
  if (header.startsWith("Rar!")) {
    return FileType::Archive;
  }
  // PDF
  if (header.startsWith("%PDF")) {
    return FileType::Document;
  }
  // GLB (Binary glTF)
  if (header.startsWith("glTF")) {
    return FileType::Model3D;
  }
  // PMD (MikuMikuDance Model)
  if (header.startsWith("Pmd")) {
    return FileType::Model3D;
  }
  // Text (simple check for ASCII)
  bool isText = true;
  for (char c : header) {
    if (c < 32 && c != 9 && c != 10 && c != 13) { // Allow tab, LF, CR
      isText = false;
      break;
    }
  }
  if (isText) {
    return FileType::Text;
  }

  return FileType::Binary;
}

FileTypeDetector::FileTypeDetector() : impl_(new Impl()) {}

FileTypeDetector::~FileTypeDetector() { delete impl_; }

FileType FileTypeDetector::detect(const QString &filePath) const {
  FileType extType = impl_->detectByExtension(filePath);
  FileType magicType = impl_->detectByMagicNumber(filePath);

  // Prefer magic number if not unknown, else use extension
  if (magicType != FileType::Unknown) {
    return magicType;
  }
  return extType;
}

FileType FileTypeDetector::detectByExtension(const QString &filePath) const {
  return impl_->detectByExtension(filePath);
}

FileType FileTypeDetector::detectByMagicNumber(const QString &filePath) const {
  return impl_->detectByMagicNumber(filePath);
}

} // namespace ArtifactCore
