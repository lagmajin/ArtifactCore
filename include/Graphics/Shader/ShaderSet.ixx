module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>


export module Graphics.Shader.Set;



export namespace ArtifactCore
{
 using namespace Diligent;
	
 struct RenderShaderPair
 {
  RefCntAutoPtr<IShader> VS;
  RefCntAutoPtr<IShader> PS;
  RefCntAutoPtr<IShader> MS;

  bool IsValid() const { return VS && PS; }
 };

};