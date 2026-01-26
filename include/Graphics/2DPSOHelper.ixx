module; 
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h> 
#include "../Define/DllExportMacro.hpp"



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

 GraphicsPipelineStateCreateInfo LIBRARY_DLL_API createDrawLinePSOHelper()
 {
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = "Line PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  auto& GPC = PSOCreateInfo.GraphicsPipeline;

  GPC.NumRenderTargets = 1;
  GPC.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  GPC.DSVFormat = TEX_FORMAT_UNKNOWN;

  GPC.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;

  GPC.RasterizerDesc.FillMode = FILL_MODE_SOLID;
  GPC.RasterizerDesc.CullMode = CULL_MODE_NONE;
  GPC.RasterizerDesc.AntialiasedLineEnable = false; // DXでは無意味だけど形式上残す
  GPC.RasterizerDesc.ScissorEnable = false;

  GPC.DepthStencilDesc.DepthEnable = false;
  GPC.DepthStencilDesc.DepthWriteEnable = false;
  GPC.DepthStencilDesc.StencilEnable = false;

  // 必要に応じて線を半透明にするなら以下を有効に
  //GPC.BlendDesc.RenderTargets[0].BlendEnable = true;

  PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  return PSOCreateInfo;
 }

 GraphicsPipelineStateCreateInfo LIBRARY_DLL_API createDrawSolidColorPSOHelper()
 {
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = "Line PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  auto& GPC = PSOCreateInfo.GraphicsPipeline;

  GPC.NumRenderTargets = 1;
  GPC.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  GPC.DSVFormat = TEX_FORMAT_UNKNOWN;

  GPC.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;

  GPC.RasterizerDesc.FillMode = FILL_MODE_SOLID;
  GPC.RasterizerDesc.CullMode = CULL_MODE_NONE;
  GPC.RasterizerDesc.AntialiasedLineEnable = false; // DXでは無意味だけど形式上残す
  GPC.RasterizerDesc.ScissorEnable = false;

  GPC.DepthStencilDesc.DepthEnable = false;
  GPC.DepthStencilDesc.DepthWriteEnable = false;
  GPC.DepthStencilDesc.StencilEnable = false;

  // 必要に応じて線を半透明にするなら以下を有効に
  //GPC.BlendDesc.RenderTargets[0].BlendEnable = true;

  PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

  return PSOCreateInfo;
 }

 GraphicsPipelineStateCreateInfo LIBRARY_DLL_API createDrawSpritePSOHelper()
 {
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = "Background PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
  auto& GPC = PSOCreateInfo.GraphicsPipeline;
  GPC.NumRenderTargets = 1;
  GPC.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  
  return PSOCreateInfo;
 }

};