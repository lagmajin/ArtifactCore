module;
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/mat.hpp>
module ImageProcessing:NegateCS;

import TextureFactory;

import Image;


namespace Diligent{}

namespace ArtifactCore
{
	class ImageF32x4RGBAWithCache;
	using namespace Diligent;

	class NegateCS::Impl
	{
	private:
	
	public:

	 RefCntAutoPtr<IRenderDevice> pDevice;
	 RefCntAutoPtr<IDeviceContext> pContext;
	 RefCntAutoPtr<IShader> pComputeShader;
	 RefCntAutoPtr<IPipelineState> pPSO;
	 RefCntAutoPtr<IShaderResourceBinding>   m_pSRB;
	 RefCntAutoPtr<ITexture>                 m_pOutputTexture;
	 QByteArray m_shaderBinaryData;


	explicit	Impl(RefCntAutoPtr<IRenderDevice> device,RefCntAutoPtr<IDeviceContext> pContext) :pDevice(device),pContext(pContext)
	{

	}

	~Impl()
	{
	}

	 void readFromByteCode(const QString& csoFilePath)
	 {
	  QFile file(csoFilePath);
	  if (!file.open(QIODevice::ReadOnly))
	  {
	   qWarning() << "Failed to open CSO file:" << csoFilePath;
	   return; // ファイルが開けなかった場合は処理を中断
	  }

	  QByteArray fileData = file.readAll(); // ファイルの内容を全て読み込む
	  file.close();

	  if (fileData.isEmpty())
	  {
	   qWarning() << "CSO file is empty:" << csoFilePath;
	   return; // ファイルが空の場合は処理を中断
	  }


	  ShaderCreateInfo ShaderCI;
	  ShaderCI.Desc.Name = "BGRA_To_RGBA_CS";
	  ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
	  ShaderCI.EntryPoint = "main"; // HLSLのバイトコードなのでエントリポイントを指定
	  ShaderCI.ByteCode     = fileData.constData(); // QByteArray の生データポインタを取得
	  ShaderCI.ByteCodeSize = fileData.size();      // QByteArray のサイズを取得
	  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

	  

	 RefCntAutoPtr<IShader> pComputeShader;


	  pDevice->CreateShader(ShaderCI, &pComputeShader);

	  ComputePipelineStateCreateInfo PSOCreateInfo;
	  PSOCreateInfo.pCS = pComputeShader;
	  PSOCreateInfo.PSODesc.Name = "MyComputePSO";
	  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	 	//ASSERT_NE(pComputeShader, nullptr);
	  ShaderResourceVariableDesc Vars[] =
	  {
	   // InputTexture はシェーダーリソース（読み取り専用）なので BIND_SHADER_RESOURCE
	   {SHADER_TYPE_COMPUTE, "InputTexture",  Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}, // Static または Mutable/Dynamic
	   // OutputTexture は順序不定アクセスビュー（書き込み用）なので BIND_UNORDERED_ACCESS
	   {SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
	  };


	  PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	  PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);



	  pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
	  
	  pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	
	 }

	 void Process(ImageF32x4RGBAWithCache cache)
	 {
	  

	 }

	 void Process(cv::Mat& mat)
	 {
	  TextureDesc InputTexDesc;
	  InputTexDesc.Name = "Input BGRA32F Texture";
	  InputTexDesc.Width = static_cast<Diligent::Uint32>(mat.cols);
	  InputTexDesc.Height = static_cast<Diligent::Uint32>(mat.rows);
	  InputTexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D; // 2Dテクスチャ
	  InputTexDesc.ArraySize = 1; // 配列ではない
	  InputTexDesc.MipLevels = 1; // ミップマップは不要
	  InputTexDesc.SampleCount = 1;
	  InputTexDesc.Format =TEX_FORMAT_RGBA32_FLOAT;
	  InputTexDesc.BindFlags =BIND_SHADER_RESOURCE;
	  InputTexDesc.Usage =USAGE_IMMUTABLE;

	  TextureData InitialData;
	  TextureSubResData subResData;
	  // ストライドは、1行あたりのバイト数です。
	  // float32x4 は 4バイト * 4チャネル = 16バイト なので、
	  // inputMat.cols * 16 となります。
	  subResData.Stride = static_cast<Diligent::Uint32>(mat.cols * sizeof(float) * 4);
	  subResData.pData = mat.data;

	  InitialData.pSubResources = &subResData;
	  InitialData.NumSubresources = 1; // ミップマップがないため1

	  //Diligent::ITexture* pTex = nullptr;
	  RefCntAutoPtr<ITexture> m_pInputTexture;
	  // テクスチャの作成
	  // エラーチェックも追加することをお勧めします。
	  pDevice->CreateTexture(InputTexDesc, &InitialData, &m_pInputTexture);


	  RefCntAutoPtr<ITexture> m_pOutputTexture;

	  TextureDesc OutputTexDesc = TextureDesc(); // 入力と同じサイズで作成
	  OutputTexDesc.Name = "Output RGBA Texture";
	  OutputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT; // 出力はRGBA8
	  OutputTexDesc.BindFlags = BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; // UAVとしてバインド可能、後でシェーダーリソースとしても読めるように
	  OutputTexDesc.Width = InputTexDesc.Width;
	  OutputTexDesc.Height = InputTexDesc.Height;
	  OutputTexDesc.Type = RESOURCE_DIM_TEX_2D;
	  OutputTexDesc.ArraySize = 1;
	  OutputTexDesc.MipLevels = 1;
	  OutputTexDesc.SampleCount = 1;
	  OutputTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
	  OutputTexDesc.Usage = USAGE_DEFAULT;

	  pDevice->CreateTexture(OutputTexDesc, nullptr, &m_pOutputTexture);

	  pContext->SetPipelineState(pPSO);
	  IShaderResourceVariable* pInputVar = m_pSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "InputTexture");
	  if (pInputVar)
	  {
	   pInputVar->Set(m_pInputTexture->GetDefaultView(TEXTURE_VIEW_TYPE::TEXTURE_VIEW_SHADER_RESOURCE));
	  }
	  else
	  {
	   LOG_ERROR("Failed to get g_InputImage variable from SRB.");
	   return;
	  }

	  // g_OutputImage に m_pOutputTexture のデフォルトUAVをバインド
	  Diligent::IShaderResourceVariable* pOutputVar = m_pSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "OutputTexture");
	  if (pOutputVar)
	  {
	   pOutputVar->Set(m_pOutputTexture->GetDefaultView(TEXTURE_VIEW_TYPE::TEXTURE_VIEW_UNORDERED_ACCESS));
	  }
	  else
	  {
	   LOG_ERROR("Failed to get g_OutputImage variable from SRB.");
	   return;
	  }

	  //pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	  pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_NONE);
	  
		Diligent::DispatchComputeAttribs Attribs;

	  const Uint32 ImageWidth = OutputTexDesc.Width;
	  const Uint32 ImageHeight = OutputTexDesc.Height;
	  Attribs.ThreadGroupCountX = (ImageWidth + 8 - 1) / 8;
	  Attribs.ThreadGroupCountY = (ImageHeight + 8 - 1) / 8;
	  Attribs.ThreadGroupCountZ = 1; // 2D画像処理なので Z は 1 のまま

	  RefCntAutoPtr<IFence> m_pFence;

	  FenceDesc fenceDesc;
	  fenceDesc.Name = "ComputeFence";

	  pDevice->CreateFence(fenceDesc, &m_pFence);
	  

	  pContext->DispatchCompute(Attribs);
	  pContext->EnqueueSignal(m_pFence, 1);

	  pContext->Flush();

	  Diligent::TextureDesc Otd = m_pOutputTexture->GetDesc();

	  Diligent::TextureDesc StagingTexDesc;
	  StagingTexDesc.Name = "Staging Texture for Readback";
	  StagingTexDesc.Width = OutputTexDesc.Width;
	  StagingTexDesc.Height = OutputTexDesc.Height;
	  StagingTexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
	  StagingTexDesc.Format = OutputTexDesc.Format; // 出力テクスチャと同じフォーマット
	  StagingTexDesc.Usage = Diligent::USAGE_STAGING;
	  StagingTexDesc.BindFlags = Diligent::BIND_NONE;
	  StagingTexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;

	  RefCntAutoPtr<ITexture> pStagingTexture;

	  pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTexture);

	  Diligent::CopyTextureAttribs CopyAttribs;
	  CopyAttribs.pSrcTexture = m_pOutputTexture;
	  CopyAttribs.SrcMipLevel = 0;
	  CopyAttribs.SrcSlice = 0;

	  CopyAttribs.pDstTexture = pStagingTexture;
	  CopyAttribs.DstMipLevel = 0;
	  CopyAttribs.DstSlice = 0;
	  CopyAttribs.DstX = 0;
	  CopyAttribs.DstY = 0;
	  CopyAttribs.DstZ = 0; // 2Dテクスチャなので0

	  Diligent::StateTransitionDesc Barriers[1];
	  Barriers[0].pResource = m_pOutputTexture;
	  Barriers[0].OldState = Diligent::RESOURCE_STATE_UNKNOWN;
	  Barriers[0].NewState = Diligent::RESOURCE_STATE_COPY_SOURCE;
	  Barriers[0].Flags = Diligent::STATE_TRANSITION_FLAG_NONE;
	  Barriers[0].FirstMipLevel = 0;
	  Barriers[0].MipLevelsCount = DILIGENT_REMAINING_MIP_LEVELS;
	  Barriers[0].FirstArraySlice = 0;
	  Barriers[0].ArraySliceCount = DILIGENT_REMAINING_ARRAY_SLICES;


	  pContext->TransitionResourceStates(_countof(Barriers), // NumBarriers: バリアの数 (Uint32)
	   Barriers);

	  pContext->CopyTexture(CopyAttribs);


	  pContext->Flush();       // コマンドをGPUに送信
	  pContext->FinishFrame();
	  Diligent::MappedTextureSubresource MappedData;

	  pContext->MapTextureSubresource(pStagingTexture, 0, 0, MAP_READ, MAP_FLAGS::MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

	  cv::Mat outputCvMat;
	  if (MappedData.pData)
	  {
	   cv::Mat tempRgbaMat(static_cast<int>(OutputTexDesc.Height),
		static_cast<int>(OutputTexDesc.Width),
		CV_32FC4,
		MappedData.pData,
		MappedData.Stride);

	   //cv::cvtColor(tempRgbaMat, outputCvMat, cv::COLOR_RGBA2BGRA);

	   pContext->UnmapTextureSubresource(pStagingTexture, 0, 0);

	  }
	 }
		
	

		

	};




	NegateCS::NegateCS(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pContext):impl_(new Impl(pDevice,pContext))
	{

	}



 NegateCS::~NegateCS()
 {

 }

 void NegateCS::Process()
 {
  //Diligent::ShaderCreateInfo ShaderCI;
  //ShaderCI.Desc.Name = "MyComputeShader";
  //ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_COMPUTE;

  //ShaderCI.EntryPoint = "main";


 }

 void NegateCS::Process(cv::Mat& mat)
 {
  impl_->Process(mat);
 }

 void NegateCS::loadShaderBinaryFile(const QString& path)
 {

 }

 void NegateCS::loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename)
 {
  if (!baseDir.exists()) {
   qWarning() << "Error: Base directory does not exist:" << baseDir.path();
   return;
  }
  qDebug() << "Base directory exists:" << baseDir.path();

  QString fullPathToShader = "";
  QDirIterator it(baseDir.path(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

  qDebug() << "Searching for file:" << filename << "in" << baseDir.path() << "and its subdirectories...";

  while (it.hasNext()) {
   QString currentPath = it.next();
   QFileInfo fileInfo(currentPath);

   if (fileInfo.fileName() == filename) {
	fullPathToShader = fileInfo.absoluteFilePath();
	qDebug() << "Found shader file at:" << fullPathToShader;
	break; // 見つかったらループを抜ける
   }
  }

  if (fullPathToShader.isEmpty()) {
   qWarning() << "Error: Shader file" << filename << "not found in" << baseDir.path() << "or its subdirectories.";
   return;
  }

  impl_->readFromByteCode(fullPathToShader);



 }

};
