module;

#include <QString>
#include <OpenImageIO/imageio.h>
export module IO.ImageImporter;

import Image;

export namespace ArtifactCore
{

 class ImageImporter {
 private:
  class Impl;
  Impl* impl_;
 public:
  ImageImporter();
  ~ImageImporter();
  bool open(const QString& filePath);
  RawImage readImage();
  void close();
 };









}