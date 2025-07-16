module;
module Graphics.Texture;

namespace ArtifactCore {

 class GPUTexture::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 GPUTexture::Impl::Impl()
 {

 }

 GPUTexture::Impl::~Impl()
 {

 }

 GPUTexture::~GPUTexture()
 {

 }

 GPUTexture::GPUTexture() :impl_(new Impl())
 {
  delete impl_;
 }

};