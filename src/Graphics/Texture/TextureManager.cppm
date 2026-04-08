module;
#include <utility>
#include <QString>
#include <string>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Graphics.Texture.Manager;


namespace ArtifactCore
{
 using namespace Diligent;

 class TextureManager::Impl
 {
 public:
  IRenderDevice* renderDevice_ = nullptr;
  Impl();
  Impl(IRenderDevice* device);
  ~Impl();
  ITexture* createTexture(const QSize& size, TEXTURE_FORMAT format, const QString& name = "");
 };

 TextureManager::Impl::Impl()
 {
 }

 TextureManager::Impl::Impl(IRenderDevice* device) : renderDevice_(device)
 {
 }

 TextureManager::Impl::~Impl()
 {
 }

 TextureManager::TextureManager() : impl_(new Impl())
 {
 }

 TextureManager::TextureManager(IRenderDevice* device) : impl_(new Impl(device))
 {
 }

 TextureManager::~TextureManager()
 {
  delete impl_;
 }

 ITexture* TextureManager::createTexture(const QSize& size, TEXTURE_FORMAT format, const QString& name)
 {
  ITexture* texture = nullptr;
  TextureDesc desc;
  std::string nameStr = name.toStdString();
  desc.Name        = nameStr.c_str();
  desc.Type        = RESOURCE_DIM_TEX_2D;
  desc.Width       = static_cast<Uint32>(size.width());
  desc.Height      = static_cast<Uint32>(size.height());
  desc.MipLevels   = 1;
  desc.Format      = format;
  desc.Usage       = USAGE_DEFAULT;
  desc.BindFlags   = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
  desc.CPUAccessFlags = CPU_ACCESS_NONE;

  if (impl_->renderDevice_)
   impl_->renderDevice_->CreateTexture(desc, nullptr, &texture);

  return texture;
 }
};
