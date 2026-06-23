module;
#include <utility>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <opencv2/opencv.hpp>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QByteArray>
#include <QString>
module ImageProcessing.SimpleChokerCS;

import Image;

namespace Diligent{}

namespace ArtifactCore
{
	class ImageF32x4RGBAWithCache;
	using namespace Diligent;

	class SimpleChokerCS::Impl
	{
	private:
	public:
	 RefCntAutoPtr<IRenderDevice> pDevice;
	 RefCntAutoPtr<IDeviceContext> pContext;
	 RefCntAutoPtr<IShader> pComputeShader;
	 RefCntAutoPtr<IPipelineState> pPSO;
	 RefCntAutoPtr<IShaderResourceBinding>   m_pSRB;
	 RefCntAutoPtr<ITexture>                 m_pOutputTexture;
	 RefCntAutoPtr<IBuffer>                 m_pCB;
	 QByteArray m_shaderBinaryData;

	explicit Impl(IRenderDevice* device, IDeviceContext* ctx)
		: pDevice(device), pContext(ctx)
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
	   return;
	  }

	  QByteArray fileData = file.readAll();
	  file.close();

	  if (fileData.isEmpty())
	  {
	   qWarning() << "CSO file is empty:" << csoFilePath;
	   return;
	  }

	  ShaderCreateInfo ShaderCI;
	  ShaderCI.Desc.Name = "SimpleChoker_CS";
	  ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
	  ShaderCI.EntryPoint = "main";
	  ShaderCI.ByteCode     = fileData.constData();
	  ShaderCI.ByteCodeSize = fileData.size();
	  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

	 RefCntAutoPtr<IShader> pComputeShader;

	  pDevice->CreateShader(ShaderCI, &pComputeShader);

	  ComputePipelineStateCreateInfo PSOCreateInfo;
	  PSOCreateInfo.pCS = pComputeShader;
	  PSOCreateInfo.PSODesc.Name = "SimpleChokerCS_PSO";
	  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

	  ShaderResourceVariableDesc Vars[] =
	  {
	   {SHADER_TYPE_COMPUTE, "InputTexture",  Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
	   {SHADER_TYPE_COMPUTE, "OutputTexture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
	   {SHADER_TYPE_COMPUTE, "Params",        Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
	  };

	  PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	  PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

	  pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
	  pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	  BufferDesc CBDesc;
	  CBDesc.Name = "SimpleChokerCB";
	  CBDesc.Size = sizeof(float) * 4;
	  CBDesc.Usage = USAGE_DYNAMIC;
	  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
	  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	  pDevice->CreateBuffer(CBDesc, nullptr, &m_pCB);
	  m_pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Params")->Set(m_pCB);
	 }

	 void Process(ImageF32x4RGBAWithCache cache) {}

	 void Process(cv::Mat& mat, float choke, int radius)
	 {
	  TextureDesc InputTexDesc;
	  InputTexDesc.Name = "Input SimpleChoker Texture";
	  InputTexDesc.Width = static_cast<Diligent::Uint32>(mat.cols);
	  InputTexDesc.Height = static_cast<Diligent::Uint32>(mat.rows);
	  InputTexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
	  InputTexDesc.ArraySize = 1;
	  InputTexDesc.MipLevels = 1;
	  InputTexDesc.SampleCount = 1;
	  InputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	  InputTexDesc.BindFlags = BIND_SHADER_RESOURCE;
	  InputTexDesc.Usage = USAGE_IMMUTABLE;

	  TextureData InitialData;
	  TextureSubResData subResData;
	  subResData.Stride = static_cast<Diligent::Uint32>(mat.cols * sizeof(float) * 4);
	  subResData.pData = mat.data;

	  InitialData.pSubResources = &subResData;
	  InitialData.NumSubresources = 1;

	  RefCntAutoPtr<ITexture> m_pInputTexture;
	  pDevice->CreateTexture(InputTexDesc, &InitialData, &m_pInputTexture);

	  RefCntAutoPtr<ITexture> m_pOutputTexture;

	  TextureDesc OutputTexDesc = TextureDesc();
	  OutputTexDesc.Name = "Output SimpleChoker Texture";
	  OutputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
	  OutputTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
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
	   LOG_ERROR("Failed to get InputTexture variable from SRB.");
	   return;
	  }

	  IShaderResourceVariable* pOutputVar = m_pSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "OutputTexture");
	  if (pOutputVar)
	  {
	   pOutputVar->Set(m_pOutputTexture->GetDefaultView(TEXTURE_VIEW_TYPE::TEXTURE_VIEW_UNORDERED_ACCESS));
	  }
	  else
	  {
	   LOG_ERROR("Failed to get OutputTexture variable from SRB.");
	   return;
	  }

	  struct { float choke; int radius; float pad[2]; } params = { choke, radius, 0, 0 };
	  pContext->UpdateBuffer(m_pCB, 0, sizeof(params), &params, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	  pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_NONE);

		Diligent::DispatchComputeAttribs Attribs;

	  const Uint32 ImageWidth = OutputTexDesc.Width;
	  const Uint32 ImageHeight = OutputTexDesc.Height;
	  Attribs.ThreadGroupCountX = (ImageWidth + 8 - 1) / 8;
	  Attribs.ThreadGroupCountY = (ImageHeight + 8 - 1) / 8;
	  Attribs.ThreadGroupCountZ = 1;

	  RefCntAutoPtr<IFence> m_pFence;

	  FenceDesc fenceDesc;
	  fenceDesc.Name = "ComputeFence";

	  pDevice->CreateFence(fenceDesc, &m_pFence);

	  pContext->DispatchCompute(Attribs);
	  pContext->EnqueueSignal(m_pFence, 1);

	  pContext->Flush();

	  TextureDesc StagingTexDesc;
	  StagingTexDesc.Name = "Staging Texture for Readback";
	  StagingTexDesc.Width = OutputTexDesc.Width;
	  StagingTexDesc.Height = OutputTexDesc.Height;
	  StagingTexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
	  StagingTexDesc.Format = OutputTexDesc.Format;
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
	  CopyAttribs.DstZ = 0;

	  Diligent::StateTransitionDesc Barriers[1];
	  Barriers[0].pResource = m_pOutputTexture;
	  Barriers[0].OldState = Diligent::RESOURCE_STATE_UNKNOWN;
	  Barriers[0].NewState = Diligent::RESOURCE_STATE_COPY_SOURCE;
	  Barriers[0].Flags = Diligent::STATE_TRANSITION_FLAG_NONE;
	  Barriers[0].FirstMipLevel = 0;
	  Barriers[0].MipLevelsCount = DILIGENT_REMAINING_MIP_LEVELS;
	  Barriers[0].FirstArraySlice = 0;
	  Barriers[0].ArraySliceCount = DILIGENT_REMAINING_ARRAY_SLICES;

	  pContext->TransitionResourceStates(_countof(Barriers), Barriers);

	  pContext->CopyTexture(CopyAttribs);

	  pContext->Flush();
	  pContext->FinishFrame();
	  MappedTextureSubresource MappedData;

	  pContext->MapTextureSubresource(pStagingTexture, 0, 0, MAP_READ, MAP_FLAGS::MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

	  cv::Mat outputCvMat;
	  if (MappedData.pData)
	  {
	   cv::Mat tempRgbaMat(static_cast<int>(OutputTexDesc.Height),
		static_cast<int>(OutputTexDesc.Width),
		CV_32FC4,
		MappedData.pData,
		MappedData.Stride);

	   pContext->UnmapTextureSubresource(pStagingTexture, 0, 0);
	  }
	 }
	};

	SimpleChokerCS::SimpleChokerCS(IRenderDevice* pDevice, IDeviceContext* pContext)
		: impl_(new Impl(pDevice, pContext))
	{
	}

	SimpleChokerCS::~SimpleChokerCS()
	{
	}

	 void SimpleChokerCS::Process(cv::Mat& mat, float choke, int radius)
	 {
	  impl_->Process(mat, choke, radius);
	 }

	 void SimpleChokerCS::loadShaderBinaryFile(const QString& path)
	 {
	 }

	 void SimpleChokerCS::loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename)
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
		break;
	   }
	  }

	  if (fullPathToShader.isEmpty()) {
	   qWarning() << "Error: Shader file" << filename << "not found in" << baseDir.path() << "or its subdirectories.";
	   return;
	  }

	  impl_->readFromByteCode(fullPathToShader);
	 }
};
