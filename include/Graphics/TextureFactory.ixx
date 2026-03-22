
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
   // �O��: mat.type() == CV_32FC4
   VERIFY_EXPR(mat.type() == CV_32FC4);
   VERIFY_EXPR(mat.isContinuous()); // ���������A�����Ă��邱�Ƃ�m�F

   // �e�N�X�`���̃T�C�Y
   const Uint32 width = static_cast<Uint32>(mat.cols);
   const Uint32 height = static_cast<Uint32>(mat.rows);

   // �e�N�X�`���L�q
   TextureDesc texDesc;
   texDesc.Name = "Float RGBA Texture";
   texDesc.Type = RESOURCE_DIM_TEX_2D;
   texDesc.Width = width;
   texDesc.Height = height;
   texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
   texDesc.Usage = USAGE_IMMUTABLE;
   texDesc.BindFlags = BIND_SHADER_RESOURCE;
   texDesc.MipLevels = 1;

   // �T�u���\�[�X����\�z
   TextureSubResData subres = {};
   subres.pData = mat.ptr();                        // �f�[�^�|�C���^
   subres.Stride = static_cast<Uint32>(mat.step);    // 1�s������̃o�C�g��
   subres.DepthStride = 0;                                // 2D�Ȃ̂ŕs�v

   TextureData initData;
   initData.pSubResources = &subres;
   initData.NumSubresources = 1;
   initData.pContext = nullptr;

   // �e�N�X�`���쐬
   RefCntAutoPtr<ITexture> pTexture;
   device->CreateTexture(texDesc, &initData, &pTexture);

   if (pTexture)
	return RefCntAutoPtr<ITextureView>{pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)};

   return {};
  }
 };















};