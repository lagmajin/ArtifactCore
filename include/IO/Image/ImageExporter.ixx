module;
//#include <OpenImageIO/>
export module IO.ImageExporter;

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