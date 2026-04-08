module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
// RefCntAutoPtr.hpp intentionally NOT included anywhere (MSVC 14.51 C1116 workaround)

export module Graphics.Resource.PSOAndSRB;

export namespace ArtifactCore
{
 using namespace Diligent;

 struct PSOAndSRB
 {
  IPipelineState*         pPSO = nullptr;
  IShaderResourceBinding* pSRB = nullptr;

  PSOAndSRB() = default;
  ~PSOAndSRB()
  {
   if (pSRB) { pSRB->Release(); pSRB = nullptr; }
   if (pPSO) { pPSO->Release(); pPSO = nullptr; }
  }
  PSOAndSRB(const PSOAndSRB& o) : pPSO(o.pPSO), pSRB(o.pSRB)
  {
   if (pPSO) pPSO->AddRef();
   if (pSRB) pSRB->AddRef();
  }
  PSOAndSRB& operator=(const PSOAndSRB& o)
  {
   if (this != &o) {
    if (pSRB) pSRB->Release();
    if (pPSO) pPSO->Release();
    pPSO = o.pPSO; pSRB = o.pSRB;
    if (pPSO) pPSO->AddRef();
    if (pSRB) pSRB->AddRef();
   }
   return *this;
  }
  PSOAndSRB(PSOAndSRB&& o) noexcept : pPSO(o.pPSO), pSRB(o.pSRB)
  {
   o.pPSO = nullptr; o.pSRB = nullptr;
  }
  PSOAndSRB& operator=(PSOAndSRB&& o) noexcept
  {
   if (this != &o) {
    if (pSRB) pSRB->Release();
    if (pPSO) pPSO->Release();
    pPSO = o.pPSO; pSRB = o.pSRB;
    o.pPSO = nullptr; o.pSRB = nullptr;
   }
   return *this;
  }
 };

};
