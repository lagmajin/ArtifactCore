module;
// RefCntAutoPtr.hpp intentionally NOT in GMF (MSVC 14.51 C1116 workaround)
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Graphics.CBuffer.Constants.Helper;
// RefCntAutoPtr.hpp intentionally NOT included (MSVC 14.51 C1116 workaround)
// CreateConstantBuffer returns IBuffer* with refcount=1; caller wraps in RefCntAutoPtr if needed.



using namespace Diligent;

export namespace ArtifactCore
{

 inline IBuffer* CreateConstantBuffer(
  IRenderDevice* device,
  Uint32 size,
  const void* initialData = nullptr, 
  const char* name = nullptr)
 {
  BufferDesc desc;
  desc.Name = name;
  desc.Usage = USAGE_DYNAMIC;
  desc.BindFlags = BIND_FLAGS::BIND_UNIFORM_BUFFER;
  desc.CPUAccessFlags = CPU_ACCESS_WRITE;
  desc.Size = size;

  BufferData buffData;
  buffData.pData = initialData;
  buffData.DataSize = size;
  buffData.pContext = nullptr;

  IBuffer* buffer = nullptr;
  device->CreateBuffer(desc, initialData ? &buffData : nullptr, &buffer);
  return buffer;
 }

}

