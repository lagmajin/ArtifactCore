module;
#include <QFileInfo>
#include <QString>

export module File.Preview;

import File.TypeDetector;

export namespace ArtifactCore {

 struct PreviewDescriptor final {
  QString filePath;
  QString displayName;
  FileType fileType = FileType::Unknown;
  bool exists = false;
  bool isDirectory = false;
  bool readable = false;
  bool canPreviewInternally = false;
  bool canPreviewWithSystem = false;

  bool isPreviewable() const {
   return canPreviewInternally || canPreviewWithSystem;
  }
 };

 inline bool isInternalPreviewType(FileType type) {
  switch (type) {
  case FileType::Image:
  case FileType::Video:
  case FileType::Audio:
  case FileType::Text:
  case FileType::Document:
  case FileType::Model3D:
   return true;
  case FileType::Unknown:
  case FileType::Binary:
  case FileType::Archive:
  default:
   return false;
  }
 }

 inline PreviewDescriptor makePreviewDescriptor(const QString& filePath) {
  PreviewDescriptor descriptor;
  descriptor.filePath = filePath;

  const QFileInfo info(filePath);
  descriptor.exists = info.exists();
  descriptor.isDirectory = info.isDir();
  descriptor.readable = info.isReadable();
  descriptor.displayName = info.fileName().isEmpty() ? filePath : info.fileName();

  if (descriptor.exists && !descriptor.isDirectory) {
   const QString absolutePath = info.absoluteFilePath();
   FileTypeDetector detector;
   descriptor.fileType = detector.detect(absolutePath);
   descriptor.filePath = absolutePath;
   descriptor.canPreviewInternally = isInternalPreviewType(descriptor.fileType);
   descriptor.canPreviewWithSystem = descriptor.readable;
  }

  return descriptor;
 }

}
