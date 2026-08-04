module;

#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <wobjectdefs.h>
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
#include <cstdint>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
export module Image.ImageF32x4RGBAWithCache;

import Memory.SharedPtr;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore
{
class LIBRARY_DLL_API ImageF32x4RGBAWithCache:public QObject{
W_OBJECT(ImageF32x4RGBAWithCache)
private:
class Impl;
Impl* impl_;
public:
ImageF32x4RGBAWithCache();
explicit ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image);
ImageF32x4RGBAWithCache(const ImageF32x4RGBAWithCache& other);
~ImageF32x4RGBAWithCache();
ImageF32x4_RGBA& image() const;
void SetCpuImage(const ImageF32x4_RGBA& image);

void UpdateGpuTextureFromCpuData();
void UpdateCpuDataFromGpuTexture();
// Call after a compute/render pass writes through the UAV view.
void MarkGpuDataDirty();
// Returns the current shader-resource view, uploading CPU changes first.
Diligent::RefCntAutoPtr<Diligent::ITextureView> GetGpuTextureSRV(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);
// Returns the current unordered-access view, uploading CPU changes first.
Diligent::RefCntAutoPtr<Diligent::ITextureView> GetGpuTextureUAV(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);
void UpdateCpuRegion(const ImageF32x4_RGBA& source, int x, int y,
                     uint32_t width, uint32_t height);
void SetGpuResources(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                     Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);

int32_t width() const;
int32_t height() const;
bool IsGpuTextureValid() const;
  
// Deep copy methods
ImageF32x4RGBAWithCache DeepCopy() const;
ImageF32x4RGBAWithCache& operator=(const ImageF32x4RGBAWithCache& other);


 };

// Recommended: Use this alias for shared image ownership.
// Example: ImageF32x4RGBAWithCachePtr myImage = makeShared<ImageF32x4RGBAWithCache>();
using ImageF32x4RGBAWithCachePtr = SharedPtr<ImageF32x4RGBAWithCache>;














};
