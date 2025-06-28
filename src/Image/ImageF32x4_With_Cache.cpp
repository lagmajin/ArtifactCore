module;
#include <wobjectimpl.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
module Image:ImageF32x4_With_Cache;

import Image;

namespace Diligent{}//dummy

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
  if (m_bGpuDataDirty)
  {
   //UpdateCpuDataFromGpuTexture(pContext); // pContext を渡す
  }
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
  // EnsureGpuTextureReady(); // Uncomment if you have texture initialization logic here

  if (m_bCpuDataDirty)
  {
   // IMPORTANT: Review this function name and its actual implementation.
   // If m_bCpuDataDirty means CPU has new data for GPU, this should be a 'GPU update' function.
   // Example: m_pContext->UpdateTexture(m_pGpuTexture, 0, 0, 0, ...)
   UpdateCpuDataFromGpuTexture();
  }

  // Transition the texture state to Unordered Access before using it as a UAV
  //m_pContext->TransitionShaderResourceStates(m_pGpuTexture, Diligent::RESOURCE_STATE_UNORDERED_ACCESS);

  // Explicitly construct RefCntAutoPtr from the raw pointer returned by GetDefaultView()
  if (m_pGpuTexture)
  {
   // GetDefaultView が ITextureView* を返す場合:
   ITextureView* pView = m_pGpuTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
   return RefCntAutoPtr<ITextureView>(pView); // 生ポインタをRefCntAutoPtrでラップ
  }
  else
  {
   return RefCntAutoPtr<ITextureView>(nullptr);
  }
 }

 void ImageF32x4RGBAWithCache::Impl::UpdateGpuTextureFromCpuData()
 {

 }

 void ImageF32x4RGBAWithCache::Impl::UpdateCpuDataFromGpuTexture()
 {

 }

ImageF32x4RGBAWithCache::ImageF32x4RGBAWithCache():impl_(new Impl())
 {

 }

ImageF32x4RGBAWithCache::ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image) :impl_(new Impl())
{

}

 ImageF32x4RGBAWithCache::~ImageF32x4RGBAWithCache()
 {
  delete impl_;
 }

 void ImageF32x4RGBAWithCache::UpdateGpuTextureFromCpuData()
 {

 }

 void ImageF32x4RGBAWithCache::UpdateCpuDataFromGpuTexture()
 {

 }

};