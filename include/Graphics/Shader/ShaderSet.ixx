module;
#include <utility>
// RefCntAutoPtr.hpp intentionally NOT included anywhere (MSVC 14.51 C1116 workaround)
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>


export module Graphics.Shader.Set;

export namespace ArtifactCore
{
 using namespace Diligent;

 struct RenderShaderPair
 {
  IShader* VS = nullptr;
  IShader* PS = nullptr;
  IShader* MS = nullptr;

  bool IsValid() const { return VS && PS; }

  RenderShaderPair() = default;
  ~RenderShaderPair()
  {
   if (MS) { MS->Release(); MS = nullptr; }
   if (PS) { PS->Release(); PS = nullptr; }
   if (VS) { VS->Release(); VS = nullptr; }
  }
  RenderShaderPair(const RenderShaderPair& o) : VS(o.VS), PS(o.PS), MS(o.MS)
  {
   if (VS) VS->AddRef();
   if (PS) PS->AddRef();
   if (MS) MS->AddRef();
  }
  RenderShaderPair& operator=(const RenderShaderPair& o)
  {
   if (this != &o) {
    if (MS) MS->Release(); if (PS) PS->Release(); if (VS) VS->Release();
    VS = o.VS; PS = o.PS; MS = o.MS;
    if (VS) VS->AddRef(); if (PS) PS->AddRef(); if (MS) MS->AddRef();
   }
   return *this;
  }
  RenderShaderPair(RenderShaderPair&& o) noexcept : VS(o.VS), PS(o.PS), MS(o.MS)
  {
   o.VS = nullptr; o.PS = nullptr; o.MS = nullptr;
  }
  RenderShaderPair& operator=(RenderShaderPair&& o) noexcept
  {
   if (this != &o) {
    if (MS) MS->Release(); if (PS) PS->Release(); if (VS) VS->Release();
    VS = o.VS; PS = o.PS; MS = o.MS;
    o.VS = nullptr; o.PS = nullptr; o.MS = nullptr;
   }
   return *this;
  }
 };

};
