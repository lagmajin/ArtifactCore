module;
//#include <OpenImageIO/>

#include <QDir>
#include <QString>
#include <QObject>
export module IO.ImageExporter;

import Image;

namespace OIIO {};//dummy

export namespace ArtifactCore {

 using namespace OIIO;

 class ImageExporter
 {
 private:


 public:
  ImageExporter();
  ~ImageExporter();
  bool write();
 };





}