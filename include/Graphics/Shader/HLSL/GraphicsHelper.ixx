module;
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Platforms/Basic/interface/DebugUtilities.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <opencv2/core/mat.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <QDebug>
#include "QString"


export module Graphics:GraphicsHelper;


export namespace ArtifactCore {

 using namespace Diligent;

 auto CreateComputeShaderInfoFromByteCode(const QString&name,const char*entryName,size_t byteCodeSize,SHADER_SOURCE_LANGUAGE language)
 {
  ShaderCreateInfo info;
  info.Desc.ShaderType = SHADER_TYPE_COMPUTE;
  info.EntryPoint =entryName ;



  return info;
 }


 DispatchComputeAttribs CreateDispatchAttribs(
  Uint32 TotalWidth,
  Uint32 TotalHeight,
  Uint32 TotalDepth = 1,
  Uint32 NumThreadsPerGroupX = 64,
  Uint32 NumThreadsPerGroupY = 1,
  Uint32 NumThreadsPerGroupZ = 1
 )
 {
  DispatchComputeAttribs Attribs;
  Attribs.ThreadGroupCountX = (TotalWidth + NumThreadsPerGroupX - 1) / NumThreadsPerGroupX;
  Attribs.ThreadGroupCountY = (TotalHeight + NumThreadsPerGroupY - 1) / NumThreadsPerGroupY;
  Attribs.ThreadGroupCountZ = (TotalDepth + NumThreadsPerGroupZ - 1) / NumThreadsPerGroupZ;
  return Attribs;
 }

 StateTransitionDesc CreateTransitionDescForCS(
  Diligent::ITexture* pTexture, // ITexture に特化
  Diligent::RESOURCE_STATE NewState,
  Diligent::RESOURCE_STATE OldState = Diligent::RESOURCE_STATE_UNKNOWN, // デフォルトでUNKNOWNから遷移
  Diligent::STATE_TRANSITION_FLAGS TransitionFlags = Diligent::STATE_TRANSITION_FLAG_NONE
 )
 {
  // NewState のチェック（より堅牢にするなら）
  VERIFY_EXPR(NewState == Diligent::RESOURCE_STATE_SHADER_RESOURCE ||
   NewState == Diligent::RESOURCE_STATE_UNORDERED_ACCESS);

  Diligent::StateTransitionDesc Desc;
  Desc.pResource = pTexture;
  Desc.OldState = OldState;
  Desc.NewState = NewState;
  Desc.Flags = TransitionFlags;

  // 必要に応じて、特定の CS 関連フラグを自動的に追加することも検討可能。
  // 例:
  // if (NewState == Diligent::RESOURCE_STATE_SHADER_RESOURCE)
  // {
  //     Desc.TransitionFlags |= Diligent::STATE_TRANSITION_FLAG_TRANSITION_SHADER_INPUT; // このフラグはSRVへの遷移時に役立つ
  // }

  return Desc;
 }
 
 TextureDesc CreateCSInputTextureDesc(
  Uint32 width,
  Uint32 height,
  TEXTURE_FORMAT  format,
  const char* name
 ) {
  TextureDesc desc;
  desc.Name = name ? name : "CSInputTexture";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_SHADER_RESOURCE;  // 読み取り専用
  desc.CPUAccessFlags = CPU_ACCESS_NONE;
  desc.MiscFlags = Diligent::MISC_TEXTURE_FLAG_NONE;

  return desc;
 }

 TextureDesc CreateCSInputTextureDesc(
 cv::Mat& mat,
  TEXTURE_FORMAT  format,
  const char* name
 ) {
  TextureDesc desc;
  desc.Name = name ? name : "CSInputTexture";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = mat.cols;
  desc.Height = mat.rows;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_SHADER_RESOURCE;  // 読み取り専用
  desc.CPUAccessFlags = CPU_ACCESS_NONE;
  desc.MiscFlags = Diligent::MISC_TEXTURE_FLAG_NONE;

  return desc;
 }


 TextureDesc CreateCSOutputTextureDesc(
  Uint32                               Width,
  Uint32                               Height,
  TEXTURE_FORMAT             Format =TEX_FORMAT_RGBA32_FLOAT, // よく使うRGBA8をデフォルトに
  Uint32                               MipLevels = 1, // CS出力でミップマップは通常不要
  const char* Name = "CSOutputTexture" // デフォルト名
 )
 {
  TextureDesc TexDesc;

  TexDesc.Name = Name;
  TexDesc.Type = RESOURCE_DIM_TEX_2D;
  TexDesc.Width = Width;
  TexDesc.Height = Height;
  TexDesc.Format = Format;
  TexDesc.MipLevels = MipLevels;
  TexDesc.ArraySize = 1; // 2Dテクスチャアレイではない
  TexDesc.Usage = USAGE_DEFAULT; // GPUからのみアクセス
  // CS出力として必須: BIND_UNORDERED_ACCESS (UAV)
  // 後続処理でシェーダー入力として使うため: BIND_SHADER_RESOURCE (SRV)
  TexDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE;
  TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE; // CPUからはアクセスしない
  TexDesc.SampleCount = 1; // マルチサンプリングなし
  TexDesc.MiscFlags = Diligent::MISC_TEXTURE_FLAG_NONE; // その他のフラグはなし

  return TexDesc;
 }

 RefCntAutoPtr<ITexture> CreateCSSRVInputTexture(Uint32 Width,
  Uint32 Height, 
  TEXTURE_FORMAT   Format = TEX_FORMAT_RGBA32_FLOAT,
  QString name=""
  )
 {


  return RefCntAutoPtr<ITexture>(nullptr);
 }

 RefCntAutoPtr<ITexture> CreateCSSRVInputTexture(Uint32 Width,
  Uint32 Height,
  const QString name,
  TEXTURE_FORMAT   Format = TEX_FORMAT_RGBA32_FLOAT
 
 )
 {


  return RefCntAutoPtr<ITexture>(nullptr);
 }


 auto CreateSRVInputTexture(IRenderDevice* device, cv::Mat& mat,
  const QString& name,
  TEXTURE_FORMAT   Format = TEX_FORMAT_RGBA32_FLOAT
 )
 {
  if (mat.channels() != 4 || mat.type() != CV_32FC4) {
   qWarning() << "Expected CV_32FC4 format (RGBA32F)";
   return RefCntAutoPtr<ITexture>(nullptr);
  }
  TextureDesc texDesc;
  texDesc.Name = name.toUtf8().constData();
  texDesc.Type = RESOURCE_DIM_TEX_2D;
  texDesc.Width = mat.cols;
  texDesc.Height = mat.rows;
  texDesc.Format = Format;
  texDesc.Usage = USAGE_IMMUTABLE; // 読み取り専用
  texDesc.BindFlags = BIND_SHADER_RESOURCE;
  texDesc.MipLevels = 1;


  TextureData InitialData;
  TextureSubResData subResData;

  subResData.Stride = static_cast<Uint32>(mat.step);
  subResData.pData = mat.data;

  InitialData.pSubResources = &subResData;
  InitialData.NumSubresources = 1; // ミップマップがないため1

  RefCntAutoPtr<ITexture> texture;
  device->CreateTexture(texDesc, &InitialData, &texture);

  if (!texture)

  return RefCntAutoPtr<ITexture>(nullptr);
 }
 
 auto CreateCSOutputTexture()
 {

  return RefCntAutoPtr<ITexture>(nullptr);
 }

}
