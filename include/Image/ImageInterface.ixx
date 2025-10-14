module;

export module ImageInterface;

export namespace ArtifactCore
{

 class ImageInterface
 {
 private:

 public:
  ImageInterface();
  ~ImageInterface();

  virtual int width() const = 0;
  virtual int height() const = 0;
 };








};