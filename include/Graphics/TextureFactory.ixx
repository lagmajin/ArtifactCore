
module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>
// RefCntAutoPtr.hpp intentionally NOT in GMF (MSVC 14.51 C1116 workaround)
#include <DiligentCore/Platforms/Basic/interface/DebugUtilities.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <opencv2/core/mat.hpp>
export module TextureFactory;

// RefCntAutoPtr.hpp intentionally NOT included (MSVC 14.51 C1116 workaround)
// CreateFromFloatRGBA returns ITextureView* (borrowed); AddRef if you need to own it.

namespace Diligent{}

export namespace ArtifactCore
{
 using namespace Diligent;

 class TextureFactory {
 public:
  static ITextureView* CreateFromFloatRGBA(IRenderDevice* device, const cv::Mat& mat)
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
   ITexture* pTexture = nullptr;
   device->CreateTexture(texDesc, &initData, &pTexture);

   if (pTexture)
	return pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

   return nullptr;
  }
 };

}















