module;
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
module File.TypeDetector;



import std;

namespace ArtifactCore {

 class FileTypeDetector::Impl {
 public:
  Impl();
  ~Impl();

  FileType detectByExtension(const QString& filePath) const;
  FileType detectByMagicNumber(const QString& filePath) const;
 };

 FileTypeDetector::Impl::Impl()
 {

 }

 FileTypeDetector::Impl::~Impl()
 {

 }

 FileType FileTypeDetector::Impl::detectByExtension(const QString& filePath) const
 {
  QFileInfo fileInfo(filePath);
  QString suffix = fileInfo.suffix().toLower();

  if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "bmp" || suffix == "gif" || suffix == "tiff") {
    return FileType::Image;
  } else if (suffix == "mp4" || suffix == "avi" || suffix == "mov" || suffix == "mkv" || suffix == "wmv") {
    return FileType::Video;
  } else if (suffix == "mp3" || suffix == "wav" || suffix == "flac" || suffix == "aac" || suffix == "ogg") {
    return FileType::Audio;
  } else if (suffix == "txt" || suffix == "md" || suffix == "json" || suffix == "xml" || suffix == "html") {
    return FileType::Text;
  } else if (suffix == "pdf" || suffix == "doc" || suffix == "docx" || suffix == "xls" || suffix == "xlsx") {
    return FileType::Document;
  } else if (suffix == "zip" || suffix == "rar" || suffix == "7z" || suffix == "tar" || suffix == "gz") {
    return FileType::Archive;
  } else {
    return FileType::Unknown;
  }
 }

 FileType FileTypeDetector::Impl::detectByMagicNumber(const QString& filePath) const
 {
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
  if (header.startsWith("ID3") || header.startsWith("\xFF\xFB") || header.startsWith("\xFF\xF3") || header.startsWith("\xFF\xF2")) {
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

 FileTypeDetector::FileTypeDetector() : impl_(new Impl())
 {

 }

 FileTypeDetector::~FileTypeDetector()
 {
  delete impl_;
 }

 FileType FileTypeDetector::detect(const QString& filePath) const
 {
  FileType extType = impl_->detectByExtension(filePath);
  FileType magicType = impl_->detectByMagicNumber(filePath);

  // Prefer magic number if not unknown, else use extension
  if (magicType != FileType::Unknown) {
    return magicType;
  }
  return extType;
 }

 FileType FileTypeDetector::detectByExtension(const QString& filePath) const
 {
  return impl_->detectByExtension(filePath);
 }

 FileType FileTypeDetector::detectByMagicNumber(const QString& filePath) const
 {
  return impl_->detectByMagicNumber(filePath);
 }

}