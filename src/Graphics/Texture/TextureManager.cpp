module;
#include <QString>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
module Graphics.Texture.Manager;


namespace ArtifactCore
{
 using namespace Diligent;
	
 class TextureManager::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> renderDevice_;
 public:
  Impl();
  Impl(RefCntAutoPtr<IRenderDevice> device);
  ~Impl();
  RefCntAutoPtr<ITexture> createTexture(const QSize& size, TEXTURE_FORMAT format, const QString& name = "");
 };

 TextureManager::Impl::Impl()
 {
 }

 TextureManager::Impl::Impl(RefCntAutoPtr<IRenderDevice> device) :renderDevice_(device)
 {
 }

 TextureManager::Impl::~Impl()
 {
 }

 TextureManager::TextureManager():impl_(new Impl())
 {

 }

 TextureManager::TextureManager(RefCntAutoPtr<IRenderDevice>& device) :impl_(new Impl(device))
 {
 }
	
 TextureManager::~TextureManager()
 {
  delete impl_;
 }

 RefCntAutoPtr<ITexture> TextureManager::createTexture(const QSize& size, TEXTURE_FORMAT format,const QString& name)
 {
  RefCntAutoPtr<ITexture> texture;
  TextureDesc desc;
  desc.Name = name.toStdString().c_str();
  desc.Type = RESOURCE_DIM_TEX_2D;
  //desc.Width = width;
  //desc.Height = height;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
  desc.CPUAccessFlags = CPU_ACCESS_NONE;

 	
  return texture;
 }
};
