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
	   return; // �t�@�C�����J���Ȃ������ꍇ�͏����𒆒f
	  }

	  QByteArray fileData = file.readAll(); // �t�@�C���̓��e��S�ēǂݍ���
	  file.close();

	  if (fileData.isEmpty())
	  {
	   qWarning() << "CSO file is empty:" << csoFilePath;
	   return; // �t�@�C������̏ꍇ�͏����𒆒f
	  }


	  Diligent::ShaderCreateInfo ShaderCI;
	  ShaderCI.Desc.Name = "BGRA_To_RGBA_CS";
	  ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_COMPUTE;
	  ShaderCI.EntryPoint = "main"; // HLSL�̃o�C�g�R�[�h�Ȃ̂ŃG���g���|�C���g���w��
	  Diligent::RefCntAutoPtr<Diligent::IShader> pComputeShader;
	  device->CreateShader(ShaderCI, &pComputeShader);

	  ComputePipelineStateCreateInfo PSOCreateInfo;
	  PSOCreateInfo.PSODesc.Name = "MyComputePSO";
	  PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_COMPUTE;


	 	//ASSERT_NE(pComputeShader, nullptr);

		/*
	  ShaderResourceVariableDesc Vars[] =
	  {
		  {Diligent::SHADER_TYPE_COMPUTE, "InputBGRA_Texture", Diligent::SHADER_RESOURCE_VARIABLE_FLAG_NONE}, // �f�t�H���g�� Mutable
		  {Diligent::SHADER_TYPE_COMPUTE, "OutputRGBA_Texture", Diligent::SHADER_RESOURCE_VARIABLE_FLAG_NONE}
	  };

		*/
	  //Diligent::StaticSamplerDesc Samplers[] = {};

	  //PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
	  //PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
	  PSOCreateInfo.pCS = pComputeShader;

	  device->CreateComputePipelineState(PSOCreateInfo, &pPSO);
	  //ASSERT_NE(m_pPSO, nullptr);

	  // 4. �V�F�[�_�[���\�[�X�o�C���f�B���O (SRB) �̍쐬
	  // SRB��PSO�Ɗ֘A�t�����A���ۂɃ��\�[�X�r���[��ݒ肷�邽�߂Ɏg�p����܂��B
	  pPSO->CreateShaderResourceBinding(&m_pSRB, true);

	  TextureDesc OutputTexDesc =TextureDesc(); // ���͂Ɠ����T�C�Y�ō쐬
	  OutputTexDesc.Name = "Output RGBA Texture";
	  OutputTexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM; // �o�͂�RGBA8
	  OutputTexDesc.BindFlags = Diligent::BIND_UNORDERED_ACCESS | Diligent::BIND_SHADER_RESOURCE; // UAV�Ƃ��ăo�C���h�\�A��ŃV�F�[�_�[���\�[�X�Ƃ��Ă��ǂ߂�悤��
	  device->CreateTexture(OutputTexDesc, nullptr, &m_pOutputTexture);
	  //ASSERT_NE(m_pOutputTexture, nullptr);

	  //ITextureView* pInputSRV = m_pInputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
	  ITextureView* pOutputUAV = m_pOutputTexture->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);


	  // SRB�Ƀ��\�[�X���Z�b�g
	  // �V�F�[�_�[���̕ϐ����ƈ�v������K�v������܂�
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
