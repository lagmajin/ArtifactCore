
module;
#include <QSizeF>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../Define/DllExportMacro.hpp"
export module Graphics.Texture.Manager;

export namespace ArtifactCore {

 using namespace Diligent;

 class LIBRARY_DLL_API TextureManager {
 private:
  class Impl;
  Impl* impl_;
 public:
  TextureManager();
  TextureManager(RefCntAutoPtr<IRenderDevice>& device);
  ~TextureManager();
  RefCntAutoPtr<ITexture> createTexture(const QSize&size,TEXTURE_FORMAT format,const QString& name="");
 };






}