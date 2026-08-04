module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
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
#include <cstring>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QObject>
#include <wobjectimpl.h>
module Image.ImageF32x4RGBAWithCache;

import Image.ImageF32x4_RGBA;

namespace Diligent {}//dummy

namespace ArtifactCore
{
 using namespace Diligent;

 W_OBJECT_IMPL(ImageF32x4RGBAWithCache)

  class ImageF32x4RGBAWithCache::Impl
 {
 private:
  void CreateGpuTextureInternal(RefCntAutoPtr<IRenderDevice> pDevice, uint32_t width, uint32_t height);
  void UnionDirtyBox(Diligent::Box& currentBox, int x, int y, uint32_t width, uint32_t height);
  void ResetDirtyBox(Diligent::Box& box);
 public:
  ImageF32x4_RGBA m_cpuImage;
  Diligent::RefCntAutoPtr<ITexture> m_pGpuTexture;
  Diligent::RefCntAutoPtr<IRenderDevice> m_pDevice;
  Diligent::RefCntAutoPtr<IDeviceContext> m_pContext;
  bool m_bCpuDataDirty = false;
  // GPUデータが変更されたことを示すフラグ（CPUへの同期が必要）
  bool m_bGpuDataDirty = false;
  Diligent::Box m_cpuDirtyBox;
  const ImageF32x4_RGBA& GetCpuImage() const;
  ImageF32x4_RGBA& GetCpuImageMutable();
  void SetCpuImage(const ImageF32x4_RGBA& newImage);
  void UpdateCpuRegion(const ImageF32x4_RGBA& newData, int x, int y, uint32_t width, uint32_t height);

  RefCntAutoPtr<ITextureView> GetGpuTextureSRV(RefCntAutoPtr<IDeviceContext> pContext);

  RefCntAutoPtr<ITextureView> GetGpuTextureUAV(RefCntAutoPtr<IDeviceContext> pContext);

  void UpdateGpuTextureFromCpuData();
  void UpdateCpuDataFromGpuTexture();
 };

 void ImageF32x4RGBAWithCache::Impl::CreateGpuTextureInternal(RefCntAutoPtr<IRenderDevice> pDevice, uint32_t width, uint32_t height)
 {
  if (!pDevice || width == 0 || height == 0 || !m_cpuImage.rgba32fData()) return;
  TextureDesc desc;
  desc.Name = "ImageF32x4RGBAWithCache";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = width;
  desc.Height = height;
  desc.ArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = TEX_FORMAT_RGBA32_FLOAT;
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
  TextureSubResData subresource;
  subresource.pData = m_cpuImage.rgba32fData();
  subresource.Stride = static_cast<Uint64>(width) * 4u * sizeof(float);
  TextureData initialData;
  initialData.pSubResources = &subresource;
  initialData.NumSubresources = 1;
  pDevice->CreateTexture(desc, &initialData, &m_pGpuTexture);
  if (m_pGpuTexture) {
   m_bCpuDataDirty = false;
   m_bGpuDataDirty = false;
   m_pDevice = pDevice;
  }
 }

 const ImageF32x4_RGBA& ImageF32x4RGBAWithCache::Impl::GetCpuImage() const
 {
  return m_cpuImage;
 }

 ImageF32x4_RGBA& ImageF32x4RGBAWithCache::Impl::GetCpuImageMutable()
 {
  m_bCpuDataDirty = true; // 可変参照を返すので、変更されたと見なす
  ResetDirtyBox(m_cpuDirtyBox); // 全体がダーティとマーク
  return m_cpuImage;
 }

 RefCntAutoPtr<ITextureView> ImageF32x4RGBAWithCache::Impl::GetGpuTextureSRV(RefCntAutoPtr<IDeviceContext> pContext)
 {
  if (pContext) m_pContext = pContext;
  if (m_bCpuDataDirty && m_pDevice && m_pContext)
   UpdateGpuTextureFromCpuData();
  // pContext を使って状態遷移を行う
  //pContext->TransitionShaderResourceStates(m_pGpuTexture, Diligent::RESOURCE_STATE_SHADER_RESOURCE);

  if (m_pGpuTexture)
  {
   ITextureView* pView = m_pGpuTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
   return RefCntAutoPtr<ITextureView>(pView);
  }
  else
  {
   return RefCntAutoPtr<ITextureView>(nullptr);
  }
 }

 RefCntAutoPtr<ITextureView> ImageF32x4RGBAWithCache::Impl::GetGpuTextureUAV(RefCntAutoPtr<IDeviceContext> pContext)
 {
  if (pContext) m_pContext = pContext;
  if (m_bCpuDataDirty && m_pDevice && m_pContext)
   UpdateGpuTextureFromCpuData();

  // Transition the texture state to Unordered Access before using it as a UAV
  //m_pContext->TransitionShaderResourceStates(m_pGpuTexture, Diligent::RESOURCE_STATE_UNORDERED_ACCESS);

  // Explicitly construct RefCntAutoPtr from the raw pointer returned by GetDefaultView()
  if (m_pGpuTexture)
  {
   // GetDefaultView が ITextureView* を返す場合:
   ITextureView* pView = m_pGpuTexture->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
   return RefCntAutoPtr<ITextureView>(pView); // 生ポインタをRefCntAutoPtrでラップ
  }
  else
  {
   return RefCntAutoPtr<ITextureView>(nullptr);
  }
 }

 void ImageF32x4RGBAWithCache::Impl::UpdateGpuTextureFromCpuData()
 {
  if (!m_pDevice || !m_pContext || m_cpuImage.width() <= 0 || m_cpuImage.height() <= 0 ||
      !m_cpuImage.rgba32fData()) return;
  if (!m_pGpuTexture || m_pGpuTexture->GetDesc().Width != static_cast<Uint32>(m_cpuImage.width()) ||
      m_pGpuTexture->GetDesc().Height != static_cast<Uint32>(m_cpuImage.height())) {
   CreateGpuTextureInternal(m_pDevice, static_cast<uint32_t>(m_cpuImage.width()),
                            static_cast<uint32_t>(m_cpuImage.height()));
   return;
  }
  TextureSubResData subresource;
  subresource.pData = m_cpuImage.rgba32fData();
  subresource.Stride = static_cast<Uint64>(m_cpuImage.width()) * 4u * sizeof(float);
  if (m_cpuDirtyBox.MaxX <= m_cpuDirtyBox.MinX ||
      m_cpuDirtyBox.MaxY <= m_cpuDirtyBox.MinY)
   ResetDirtyBox(m_cpuDirtyBox);
  m_pContext->UpdateTexture(m_pGpuTexture, 0, 0, m_cpuDirtyBox, subresource,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  m_bCpuDataDirty = false;
  m_bGpuDataDirty = false;
  m_cpuDirtyBox.MinX = m_cpuDirtyBox.MaxX = 0;
  m_cpuDirtyBox.MinY = m_cpuDirtyBox.MaxY = 0;
  m_cpuDirtyBox.MinZ = m_cpuDirtyBox.MaxZ = 0;
 }

 void ImageF32x4RGBAWithCache::Impl::UpdateCpuDataFromGpuTexture()
 {
  if (!m_pDevice || !m_pContext || !m_pGpuTexture) return;
  const auto textureDesc = m_pGpuTexture->GetDesc();
  if (textureDesc.Format != TEX_FORMAT_RGBA32_FLOAT || textureDesc.Width == 0 ||
      textureDesc.Height == 0) return;
  TextureDesc stagingDesc = textureDesc;
  stagingDesc.Name = "ImageF32x4RGBAWithCache/Readback";
  stagingDesc.Usage = USAGE_STAGING;
  stagingDesc.BindFlags = BIND_NONE;
  stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
  RefCntAutoPtr<ITexture> stagingTexture;
  m_pDevice->CreateTexture(stagingDesc, nullptr, &stagingTexture);
  if (!stagingTexture) return;
  m_pContext->CopyTexture(CopyTextureAttribs(
      m_pGpuTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      stagingTexture, RESOURCE_STATE_TRANSITION_MODE_TRANSITION));
  m_pContext->Flush();
  m_pContext->WaitForIdle();
  MappedTextureSubresource mapped{};
  m_pContext->MapTextureSubresource(stagingTexture, 0, 0, MAP_READ,
                                    MAP_FLAG_NONE, nullptr, mapped);
  if (!mapped.pData || mapped.Stride == 0) return;
  const std::size_t rowFloats = static_cast<std::size_t>(textureDesc.Width) * 4u;
  std::vector<float> packed(rowFloats * textureDesc.Height);
  for (Uint32 row = 0; row < textureDesc.Height; ++row) {
   std::memcpy(packed.data() + static_cast<std::size_t>(row) * rowFloats,
               static_cast<const char*>(mapped.pData) +
                   static_cast<std::size_t>(row) * mapped.Stride,
               rowFloats * sizeof(float));
  }
  m_pContext->UnmapTextureSubresource(stagingTexture, 0, 0);
  m_cpuImage.setFromRGBA32F(packed.data(), static_cast<int>(textureDesc.Width),
                            static_cast<int>(textureDesc.Height));
  m_bGpuDataDirty = false;
  m_bCpuDataDirty = false;
  ResetDirtyBox(m_cpuDirtyBox);
 }

 void ImageF32x4RGBAWithCache::Impl::ResetDirtyBox(Diligent::Box& box)
 {
  box.MinX = 0;
  box.MinY = 0;
  box.MinZ = 0;
  box.MaxX = static_cast<uint32_t>(std::max(0, m_cpuImage.width()));
  box.MaxY = static_cast<uint32_t>(std::max(0, m_cpuImage.height()));
  box.MaxZ = 1;
 }

 void ImageF32x4RGBAWithCache::Impl::SetCpuImage(const ImageF32x4_RGBA& newImage)
 {
  m_cpuImage = newImage.DeepCopy();
  m_bCpuDataDirty = true;
  m_bGpuDataDirty = false;
  ResetDirtyBox(m_cpuDirtyBox);
 }

 void ImageF32x4RGBAWithCache::Impl::UpdateCpuRegion(
     const ImageF32x4_RGBA& newData, const int x, const int y,
     const uint32_t width, const uint32_t height)
 {
  if (!newData.rgba32fData() || !m_cpuImage.rgba32fData() || width == 0 || height == 0 ||
      x < 0 || y < 0 || x >= m_cpuImage.width() || y >= m_cpuImage.height() ||
      width > static_cast<uint32_t>(m_cpuImage.width() - x) ||
      height > static_cast<uint32_t>(m_cpuImage.height() - y) ||
      width > static_cast<uint32_t>(newData.width()) ||
      height > static_cast<uint32_t>(newData.height())) return;
  float* destination = m_cpuImage.rgba32fData();
  const float* source = newData.rgba32fData();
  for (uint32_t row = 0; row < height; ++row) {
   const auto sourceOffset = static_cast<std::size_t>(row) * newData.width() * 4u;
   const auto destinationOffset =
       (static_cast<std::size_t>(y) + row) * m_cpuImage.width() * 4u +
       static_cast<std::size_t>(x) * 4u;
   std::memcpy(destination + destinationOffset, source + sourceOffset,
               static_cast<std::size_t>(width) * 4u * sizeof(float));
  }
  m_bCpuDataDirty = true;
  UnionDirtyBox(m_cpuDirtyBox, x, y, width, height);
 }

 void ImageF32x4RGBAWithCache::Impl::UnionDirtyBox(Diligent::Box& currentBox, int x, int y, uint32_t width, uint32_t height)
 {
  if (width == 0 || height == 0 || m_cpuImage.width() <= 0 || m_cpuImage.height() <= 0)
   return;
  const int imageWidth = m_cpuImage.width();
  const int imageHeight = m_cpuImage.height();
  const int left = std::clamp(x, 0, imageWidth);
  const int top = std::clamp(y, 0, imageHeight);
  const int right = std::clamp(x + static_cast<int>(width), 0, imageWidth);
  const int bottom = std::clamp(y + static_cast<int>(height), 0, imageHeight);
  if (left >= right || top >= bottom) return;
  if (currentBox.MaxX <= currentBox.MinX || currentBox.MaxY <= currentBox.MinY) {
   currentBox.MinX = static_cast<uint32_t>(left);
   currentBox.MinY = static_cast<uint32_t>(top);
   currentBox.MaxX = static_cast<uint32_t>(right);
   currentBox.MaxY = static_cast<uint32_t>(bottom);
   currentBox.MinZ = 0;
   currentBox.MaxZ = 1;
   return;
  }
  currentBox.MinX = std::min(currentBox.MinX, static_cast<uint32_t>(left));
  currentBox.MinY = std::min(currentBox.MinY, static_cast<uint32_t>(top));
  currentBox.MaxX = std::max(currentBox.MaxX, static_cast<uint32_t>(right));
  currentBox.MaxY = std::max(currentBox.MaxY, static_cast<uint32_t>(bottom));
  currentBox.MinZ = 0;
  currentBox.MaxZ = 1;
 }

 ImageF32x4RGBAWithCache::ImageF32x4RGBAWithCache() :impl_(new Impl())
 {
  impl_->ResetDirtyBox(impl_->m_cpuDirtyBox);
 }

 ImageF32x4RGBAWithCache::ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image) :impl_(new Impl())
 {
  impl_->SetCpuImage(image);
 }

 ImageF32x4RGBAWithCache::ImageF32x4RGBAWithCache(const ImageF32x4RGBAWithCache& other) : impl_(new Impl())
 {
  impl_->m_cpuImage = other.impl_->m_cpuImage.DeepCopy();
  impl_->m_bCpuDataDirty = other.impl_->m_bCpuDataDirty;
  impl_->m_bGpuDataDirty = false; // GPU texture is not copied
  impl_->m_cpuDirtyBox = other.impl_->m_cpuDirtyBox;
  // Note: GPU texture is not deep copied, only CPU data
 }

 ImageF32x4RGBAWithCache::~ImageF32x4RGBAWithCache()
 {
  delete impl_;
 }
 
 ImageF32x4RGBAWithCache ImageF32x4RGBAWithCache::DeepCopy() const
 {
  return ImageF32x4RGBAWithCache(*this);
 }
 
 ImageF32x4RGBAWithCache& ImageF32x4RGBAWithCache::operator=(const ImageF32x4RGBAWithCache& other)
 {
  if (this != &other) {
    impl_->m_cpuImage = other.impl_->m_cpuImage.DeepCopy();
    impl_->m_bCpuDataDirty = other.impl_->m_bCpuDataDirty;
    impl_->m_bGpuDataDirty = false; // GPU texture is not copied
    impl_->m_cpuDirtyBox = other.impl_->m_cpuDirtyBox;
    // Note: GPU texture is not copied, only CPU data
  }
  return *this;
 }

 void ImageF32x4RGBAWithCache::UpdateGpuTextureFromCpuData()
 {
  impl_->UpdateGpuTextureFromCpuData();
 }

 void ImageF32x4RGBAWithCache::SetCpuImage(const ImageF32x4_RGBA& image)
 {
  impl_->SetCpuImage(image);
 }

 void ImageF32x4RGBAWithCache::UpdateCpuDataFromGpuTexture()
 {
  impl_->UpdateCpuDataFromGpuTexture();
 }

 void ImageF32x4RGBAWithCache::MarkGpuDataDirty()
 {
  // The cache cannot observe writes performed by an external GPU pass.  Keep
  // the ownership transition explicit so a later readback cannot be skipped.
  impl_->m_bGpuDataDirty = true;
 }

 Diligent::RefCntAutoPtr<Diligent::ITextureView>
 ImageF32x4RGBAWithCache::GetGpuTextureSRV(
     Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context)
 {
  return impl_->GetGpuTextureSRV(std::move(context));
 }

 Diligent::RefCntAutoPtr<Diligent::ITextureView>
 ImageF32x4RGBAWithCache::GetGpuTextureUAV(
     Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context)
 {
  return impl_->GetGpuTextureUAV(std::move(context));
 }

 void ImageF32x4RGBAWithCache::UpdateCpuRegion(
     const ImageF32x4_RGBA& source, const int x, const int y,
     const uint32_t width, const uint32_t height)
 {
  impl_->UpdateCpuRegion(source, x, y, width, height);
 }

 void ImageF32x4RGBAWithCache::SetGpuResources(
     Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
     Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context)
 {
  impl_->m_pDevice = std::move(device);
  impl_->m_pContext = std::move(context);
  if (impl_->m_pDevice && impl_->m_cpuImage.width() > 0 && impl_->m_cpuImage.height() > 0)
   impl_->CreateGpuTextureInternal(impl_->m_pDevice,
       static_cast<uint32_t>(impl_->m_cpuImage.width()),
       static_cast<uint32_t>(impl_->m_cpuImage.height()));
 }
 
 ImageF32x4_RGBA& ImageF32x4RGBAWithCache::image() const
 {
  // A GPU pass may have written through the UAV since the last CPU read.
  // Resolve that ownership transition before exposing the mutable CPU image;
  // otherwise callers can observe stale pixels and then accidentally upload
  // them back over the GPU result.
  if (impl_->m_bGpuDataDirty && impl_->m_pDevice && impl_->m_pContext)
   impl_->UpdateCpuDataFromGpuTexture();
  // The public API returns a mutable image reference.  Route it through the
  // dirty-tracking accessor so edits made through image() are uploaded on the
  // next GPU access instead of silently leaving the texture stale.
  return impl_->GetCpuImageMutable();
 }
 
 int32_t ImageF32x4RGBAWithCache::width() const
 {
  return impl_->m_cpuImage.width();
 }
 
 int32_t ImageF32x4RGBAWithCache::height() const
 {
  return impl_->m_cpuImage.height();
 }
 
 bool ImageF32x4RGBAWithCache::IsGpuTextureValid() const
 {
  return impl_->m_pGpuTexture != nullptr;
 }

};
