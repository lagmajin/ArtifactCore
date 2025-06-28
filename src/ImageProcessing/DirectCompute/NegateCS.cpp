module;
#include <QDebug>
#include <QFile>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
module ImageProcessing:NegateCS;

import TextureFactory;

namespace Diligent{}

namespace ArtifactCore
{
 using namespace Diligent;

	class NegateCS::Impl
	{
	private:
	
	public:

	 RefCntAutoPtr<IRenderDevice> device;
	 RefCntAutoPtr<IShader> pComputeShader;
	 RefCntAutoPtr<IPipelineState> pPSO;
	 RefCntAutoPtr<Diligent::IShaderResourceBinding>   m_pSRB;
	 RefCntAutoPtr<ITexture>                 m_pOutputTexture;
	 Impl()
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


	  Diligent::ShaderCreateInfo ShaderCI;
	  ShaderCI.Desc.Name = "BGRA_To_RGBA_CS";
	  ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_COMPUTE;
	  ShaderCI.EntryPoint = "main"; // HLSLのバイトコードなのでエントリポイントを指定
	  Diligent::RefCntAutoPtr<Diligent::IShader> pComputeShader;
	  device->CreateShader(ShaderCI, &pComputeShader);

	  ComputePipelineStateCreateInfo PSOCreateInfo;
	  PSOCreateInfo.PSODesc.Name = "MyComputePSO";
	  PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_COMPUTE;


	 	//ASSERT_NE(pComputeShader, nullptr);

		/*
	  ShaderResourceVariableDesc Vars[] =
	  {
		  {Diligent::SHADER_TYPE_COMPUTE, "InputBGRA_Texture", Diligent::SHADER_RESOURCE_VARIABLE_FLAG_NONE}, // デフォルトは Mutable
		  {Diligent::SHADER_TYPE_COMPUTE, "OutputRGBA_Texture", Diligent::SHADER_RESOURCE_VARIABLE_FLAG_NONE}
	  };

		*/
	  //Diligent::StaticSamplerDesc Samplers[] = {};

	  //PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	  //PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
	  PSOCreateInfo.pCS = pComputeShader;

	  device->CreateComputePipelineState(PSOCreateInfo, &pPSO);
	  //ASSERT_NE(m_pPSO, nullptr);

	  // 4. シェーダーリソースバインディング (SRB) の作成
	  // SRBはPSOと関連付けられ、実際にリソースビューを設定するために使用されます。
	  pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	  TextureDesc OutputTexDesc =TextureDesc(); // 入力と同じサイズで作成
	  OutputTexDesc.Name = "Output RGBA Texture";
	  OutputTexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM; // 出力はRGBA8
	  OutputTexDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; // UAVとしてバインド可能、後でシェーダーリソースとしても読めるように
	  device->CreateTexture(OutputTexDesc, nullptr, &m_pOutputTexture);
	  //ASSERT_NE(m_pOutputTexture, nullptr);

	  //ITextureView* pInputSRV = m_pInputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	  ITextureView* pOutputUAV = m_pOutputTexture->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);


	  // SRBにリソースをセット
	  // シェーダー内の変数名と一致させる必要があります
	  //m_pSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "InputBGRA_Texture")->Set(pInputSRV);
	  //m_pSRB->GetVariableByName(Diligent::SHADER_TYPE_COMPUTE, "OutputRGBA_Texture")->Set(pOutputUAV);
	 }
	};



 NegateCS::NegateCS(RefCntAutoPtr<IRenderDevice> device)
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

};
