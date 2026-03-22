module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Graphics.CBuffer.Constants.Helper;





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

