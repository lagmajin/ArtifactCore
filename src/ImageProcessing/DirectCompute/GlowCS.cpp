module;
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
module ImageProcessing.GlowCS;

import Graphics;

namespace ArtifactCore {

 using namespace Diligent;

 class GlowCS::Impl
 {
 private:

 public:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pContext;
  RefCntAutoPtr<IShader> pComputeShader;
  RefCntAutoPtr<ITexture>                 pInputTexture_;
  RefCntAutoPtr<IPipelineState> pPSO;
  RefCntAutoPtr<IShaderResourceBinding>   m_pSRB;
 };




 GlowCS::GlowCS(DeviceResources& resources)
 {

 }

 GlowCS::~GlowCS()
 {

 }

 void GlowCS::Process(cv::Mat& mat, bool flip/*=false*/)
 {
  //auto inputDesc = CreateCSInputTextureDesc(mat.cols, mat.rows, TEX_FORMAT_RGBA32_FLOAT,"");

  //auto inputTexture= CreateCSInputTextureDesc(mat, "Input Texture",TEX_FORMAT_RGBA32_FLOAT);

 }

};