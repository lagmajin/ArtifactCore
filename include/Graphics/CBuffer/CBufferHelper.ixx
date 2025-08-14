module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>


export module Graphics.CBuffer.Constants.Helper;

import std;

using namespace Diligent;

export namespace ArtifactCore
{

 inline RefCntAutoPtr<IBuffer> CreateConstantBuffer(
  IRenderDevice* device,
  Uint32 size,
  const void* initialData = nullptr, 
  const char* name = nullptr)
 {
  BufferDesc desc;
  desc.Name = name;
  desc.Usage = USAGE_DYNAMIC;
  desc.BindFlags =BIND_FLAGS::BIND_UNIFORM_BUFFER;
  desc.CPUAccessFlags = CPU_ACCESS_WRITE;
  desc.Size = size;

  BufferData buffData;
  buffData.pData = initialData;
  buffData.DataSize = size;
  buffData.pContext = nullptr; // ImmediateContext が使われる

  RefCntAutoPtr<IBuffer> buffer;
  device->CreateBuffer(desc, initialData ? &buffData : nullptr, &buffer);
  return buffer;
 }








}

