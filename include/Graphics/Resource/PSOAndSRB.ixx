module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>

export module Graphics.Resource.PSOAndSRB;

export namespace ArtifactCore
{
 using namespace Diligent;
	
 struct PSOAndSRB
 {
  RefCntAutoPtr<IPipelineState> pPSO;
  RefCntAutoPtr<IShaderResourceBinding> pSRB;
 };
	

};