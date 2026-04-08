module;
#include <utility>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <QDebug>

module Graphics.Compute;

namespace ArtifactCore
{
 using namespace Diligent;

 struct ComputeExecutor::Impl
 {
  RefCntAutoPtr<IShader>               pComputeShader_;
  RefCntAutoPtr<IPipelineState>        pPSO_;
  RefCntAutoPtr<IShaderResourceBinding> pSRB_;
 };

 template <typename ResourceT>
 bool ComputeExecutor::setResource(const char* name, ResourceT* resource)
 {
  if (name == nullptr || resource == nullptr || pImpl_->pPSO_ == nullptr)
   return false;

  if (auto* staticVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_COMPUTE, name))
  {
   staticVar->Set(resource);
   return true;
  }

  if (pImpl_->pSRB_ != nullptr)
  {
   if (auto* var = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_COMPUTE, name))
   {
    var->Set(resource);
    return true;
   }
  }

  return false;
 }

 ComputeExecutor::ComputeExecutor(GpuContext& context)
  : context_(context), pImpl_(new Impl())
 {
 }

 ComputeExecutor::~ComputeExecutor()
 {
  delete pImpl_;
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

  pImpl_->pComputeShader_ = nullptr;
  pDevice->CreateShader(shaderCI, &pImpl_->pComputeShader_);
  if (pImpl_->pComputeShader_ == nullptr)
  {
   qWarning() << "ComputeExecutor: shader compilation failed for" << (desc.name ? desc.name : "(unnamed)");
   return false;
  }

  ComputePipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = desc.name;
  psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
  psoCI.pCS = pImpl_->pComputeShader_;
  psoCI.PSODesc.ResourceLayout.DefaultVariableType = desc.defaultVariableType;
  psoCI.PSODesc.ResourceLayout.Variables = desc.variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = desc.variableCount;

  pImpl_->pPSO_ = nullptr;
  pDevice->CreateComputePipelineState(psoCI, &pImpl_->pPSO_);
  if (pImpl_->pPSO_ == nullptr)
  {
   qWarning() << "ComputeExecutor: failed to create compute PSO for" << (desc.name ? desc.name : "(unnamed)");
   return false;
  }

  pImpl_->pSRB_ = nullptr;
  return true;
 }

 bool ComputeExecutor::createShaderResourceBinding(bool initializeStaticResources)
 {
  if (pImpl_->pPSO_ == nullptr)
   return false;

  pImpl_->pSRB_ = nullptr;
  pImpl_->pPSO_->CreateShaderResourceBinding(&pImpl_->pSRB_, initializeStaticResources);
  return pImpl_->pSRB_ != nullptr;
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
  if (pContext == nullptr || pImpl_->pPSO_ == nullptr || pImpl_->pSRB_ == nullptr)
   return;

  pContext->SetPipelineState(pImpl_->pPSO_);
  pContext->CommitShaderResources(pImpl_->pSRB_, transitionMode);
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
  return pImpl_->pPSO_ != nullptr && pImpl_->pSRB_ != nullptr;
 }

 IPipelineState* ComputeExecutor::pipelineState() const
 {
  return pImpl_->pPSO_;
 }

 IShaderResourceBinding* ComputeExecutor::shaderResourceBinding() const
 {
  return pImpl_->pSRB_;
 }
}
