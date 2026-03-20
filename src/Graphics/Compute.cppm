module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <QDebug>

module Graphics.Compute;

namespace ArtifactCore
{
 using namespace Diligent;

 ComputeExecutor::ComputeExecutor(GpuContext& context)
  : context_(context)
 {
 }

 bool ComputeExecutor::build(const ComputePipelineDesc& desc)
 {
  auto pDevice = context_.D3D12RenderDevice();
  if (pDevice == nullptr || desc.shaderSource == nullptr)
   return false;

  ShaderCreateInfo shaderCI;
  shaderCI.Desc.Name = desc.name;
  shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
  shaderCI.EntryPoint = desc.entryPoint;
  shaderCI.Source = desc.shaderSource;
  shaderCI.SourceLanguage = desc.sourceLanguage;

  pComputeShader_ = nullptr;
  pDevice->CreateShader(shaderCI, &pComputeShader_);
  if (pComputeShader_ == nullptr)
  {
   qWarning() << "ComputeExecutor: shader compilation failed for" << (desc.name ? desc.name : "(unnamed)");
   return false;
  }

  ComputePipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = desc.name;
  psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
  psoCI.pCS = pComputeShader_;
  psoCI.PSODesc.ResourceLayout.DefaultVariableType = desc.defaultVariableType;
  psoCI.PSODesc.ResourceLayout.Variables = desc.variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = desc.variableCount;

  pPSO_ = nullptr;
  pDevice->CreateComputePipelineState(psoCI, &pPSO_);
  if (pPSO_ == nullptr)
  {
   qWarning() << "ComputeExecutor: failed to create compute PSO for" << (desc.name ? desc.name : "(unnamed)");
   return false;
  }

  pSRB_ = nullptr;
  return true;
 }

 bool ComputeExecutor::createShaderResourceBinding(bool initializeStaticResources)
 {
  if (pPSO_ == nullptr)
   return false;

  pSRB_ = nullptr;
  pPSO_->CreateShaderResourceBinding(&pSRB_, initializeStaticResources);
  return pSRB_ != nullptr;
 }

bool ComputeExecutor::setBuffer(const char* name, IBuffer* buffer)
{
 return setResource(name, buffer);
}

bool ComputeExecutor::setBufferView(const char* name, IBufferView* view)
{
 return setResource(name, view);
}

bool ComputeExecutor::setTextureView(const char* name, ITextureView* view)
{
 return setResource(name, view);
}

 bool ComputeExecutor::setSampler(const char* name, ISampler* sampler)
 {
  return setResource(name, sampler);
 }

 void ComputeExecutor::dispatch(IDeviceContext* pContext,
  const DispatchComputeAttribs& attribs,
  RESOURCE_STATE_TRANSITION_MODE transitionMode)
 {
  if (pContext == nullptr || pPSO_ == nullptr || pSRB_ == nullptr)
   return;

  pContext->SetPipelineState(pPSO_);
  pContext->CommitShaderResources(pSRB_, transitionMode);
  pContext->DispatchCompute(attribs);
 }

 DispatchComputeAttribs ComputeExecutor::makeDispatchAttribs(
  Uint32 width,
  Uint32 height,
  Uint32 depth,
  Uint32 threadsX,
  Uint32 threadsY,
  Uint32 threadsZ)
 {
  DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX = (width + threadsX - 1) / threadsX;
  attribs.ThreadGroupCountY = (height + threadsY - 1) / threadsY;
  attribs.ThreadGroupCountZ = (depth + threadsZ - 1) / threadsZ;
  return attribs;
 }

 bool ComputeExecutor::ready() const
 {
  return pPSO_ != nullptr && pSRB_ != nullptr;
 }

 IPipelineState* ComputeExecutor::pipelineState() const
 {
  return pPSO_;
 }

 IShaderResourceBinding* ComputeExecutor::shaderResourceBinding() const
 {
  return pSRB_;
 }
}
