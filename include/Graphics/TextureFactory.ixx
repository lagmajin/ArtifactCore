
module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <opencv2/core/mat.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module TextureFactory;

namespace Diligent{}

export namespace ArtifactCore
{
 using namespace Diligent;

 class TextureFactory {
 private:

 public:
  static RefCntAutoPtr<ITextureView> CreateFromFloatRGBA(IRenderDevice* device, const cv::Mat& mat)
  {
   // 前提: mat.type() == CV_32FC4
   VERIFY_EXPR(mat.type() == CV_32FC4);
   VERIFY_EXPR(mat.isContinuous()); // メモリが連続していることを確認

   // テクスチャのサイズ
   const Uint32 width = static_cast<Uint32>(mat.cols);
   const Uint32 height = static_cast<Uint32>(mat.rows);

   // テクスチャ記述
   TextureDesc texDesc;
   texDesc.Name = "Float RGBA Texture";
   texDesc.Type = RESOURCE_DIM_TEX_2D;
   texDesc.Width = width;
   texDesc.Height = height;
   texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
   texDesc.Usage = USAGE_IMMUTABLE;
   texDesc.BindFlags = BIND_SHADER_RESOURCE;
   texDesc.MipLevels = 1;

   // サブリソース情報を構築
   TextureSubResData subres = {};
   subres.pData = mat.ptr();                        // データポインタ
   subres.Stride = static_cast<Uint32>(mat.step);    // 1行あたりのバイト数
   subres.DepthStride = 0;                                // 2Dなので不要

   TextureData initData;
   initData.pSubResources = &subres;
   initData.NumSubresources = 1;
   initData.pContext = nullptr;

   // テクスチャ作成
   RefCntAutoPtr<ITexture> pTexture;
   device->CreateTexture(texDesc, &initData, &pTexture);

   if (pTexture)
	return RefCntAutoPtr<ITextureView>{pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)};

   return {};
  }
 };















};