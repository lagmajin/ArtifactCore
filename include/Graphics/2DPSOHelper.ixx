module; 
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h> 
#include "../ArtifactWidgets/include/Define/DllExportMacro.hpp"


export module Graphics.Helper.PSO;

export namespace ArtifactCore {

 using namespace Diligent;
 
 GraphicsPipelineStateCreateInfo LIBRARY_DLL_API create2DPSOHelper()
 {
  PipelineStateDesc desc;


  GraphicsPipelineStateCreateInfo PSOCreateInfo;

  PSOCreateInfo.PSODesc.Name = "Simple PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
  

  auto& GPC = PSOCreateInfo.GraphicsPipeline;
  GPC.NumRenderTargets = 1;
  GPC.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  GPC.DSVFormat = TEX_FORMAT_UNKNOWN; 

  GPC.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  GPC.DepthStencilDesc.DepthEnable = false;
  //GPC.DepthStencilDesc.DepthWriteEnable = false;
  //GPC.DepthStencilDesc.StencilEnable = false;
  //GPC.BlendDesc.RenderTargets[0].BlendEnable = true;
  
  GPC.RasterizerDesc.FillMode = Diligent::FILL_MODE_SOLID;
  GPC.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
  GPC.RasterizerDesc.AntialiasedLineEnable = false;
  GPC.RasterizerDesc.ScissorEnable = false;

  PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
  return PSOCreateInfo;
 }






};