
module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include "../../Define/DllExportMacro.hpp"
export module Graphics.Texture.Manager;

export namespace Graphcis {

 using namespace Diligent;

 class LIBRARY_DLL_API TextureManager {
 private:
  class Impl;

 public:
  TextureManager();
  ~TextureManager();
 };

 TextureManager::TextureManager()
 {

 }

 TextureManager::~TextureManager()
 {

 }




}