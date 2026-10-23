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

module ImageProcessing.ThresholdCS;

import Image;

namespace ArtifactCore {
using namespace Diligent;

class ThresholdCS::Impl {
public:
    RefCntAutoPtr<IRenderDevice> pDevice;
    RefCntAutoPtr<IDeviceContext> pContext;
    RefCntAutoPtr<IShader> pComputeShader;
    RefCntAutoPtr<IPipelineState> pPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    RefCntAutoPtr<ITexture> m_pOutputTexture;
    RefCntAutoPtr<IBuffer> m_pCB;
    QByteArray m_shaderBinaryData;

    explicit Impl(IRenderDevice* device, IDeviceContext* ctx)
        : pDevice(device), pContext(ctx) {}

    void readFromByteCode(const QString& csoFilePath) {
        QFile file(csoFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open CSO file:" << csoFilePath;
            return;
        }
        QByteArray fileData = file.readAll();
        file.close();
        if (fileData.isEmpty()) {
            qWarning() << "CSO file is empty:" << csoFilePath;
            return;
        }

        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc.Name = "Threshold_CS";
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint = "main";
        ShaderCI.ByteCode = fileData.constData();
        ShaderCI.ByteCodeSize = fileData.size();
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        RefCntAutoPtr<IShader> pCS;
        pDevice->CreateShader(ShaderCI, &pCS);

        ComputePipelineStateCreateInfo PSOCreateInfo;
        PSOCreateInfo.pCS = pCS;
        PSOCreateInfo.PSODesc.Name = "ThresholdPSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_COMPUTE, "InputTexture",  SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_COMPUTE, "OutputTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_COMPUTE, "Params",        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
        pDevice->CreateComputePipelineState(PSOCreateInfo, &pPSO);
        pPSO->CreateShaderResourceBinding(&m_pSRB, true);

        BufferDesc CBDesc;
        CBDesc.Name = "ThresholdCB";
        CBDesc.Size = sizeof(float) * 4;
        CBDesc.Usage = USAGE_DYNAMIC;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(CBDesc, nullptr, &m_pCB);
        m_pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "Params")->Set(m_pCB);
    }

    void Process(cv::Mat& mat, float threshold, float softness) {
        if (mat.empty()) return;

        TextureDesc InputTexDesc;
        InputTexDesc.Name = "Input Texture";
        InputTexDesc.Width = static_cast<Uint32>(mat.cols);
        InputTexDesc.Height = static_cast<Uint32>(mat.rows);
        InputTexDesc.Type = RESOURCE_DIM_TEX_2D;
        InputTexDesc.ArraySize = 1;
        InputTexDesc.MipLevels = 1;
        InputTexDesc.SampleCount = 1;
        InputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        InputTexDesc.BindFlags = BIND_SHADER_RESOURCE;
        InputTexDesc.Usage = USAGE_IMMUTABLE;

        TextureData InitialData;
        TextureSubResData subResData;
        subResData.Stride = static_cast<Uint32>(mat.cols * sizeof(float) * 4);
        subResData.pData = mat.data;
        InitialData.pSubResources = &subResData;
        InitialData.NumSubresources = 1;

        RefCntAutoPtr<ITexture> m_pInputTexture;
        pDevice->CreateTexture(InputTexDesc, &InitialData, &m_pInputTexture);

        TextureDesc OutputTexDesc = {};
        OutputTexDesc.Name = "Output Texture";
        OutputTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        OutputTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
        OutputTexDesc.Width = InputTexDesc.Width;
        OutputTexDesc.Height = InputTexDesc.Height;
        OutputTexDesc.Type = RESOURCE_DIM_TEX_2D;
        OutputTexDesc.ArraySize = 1;
        OutputTexDesc.MipLevels = 1;
        OutputTexDesc.Usage = USAGE_DEFAULT;
        pDevice->CreateTexture(OutputTexDesc, nullptr, &m_pOutputTexture);

        pContext->SetPipelineState(pPSO);

        auto* pInputVar = m_pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "InputTexture");
        if (pInputVar)
            pInputVar->Set(m_pInputTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        auto* pOutputVar = m_pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "OutputTexture");
        if (pOutputVar)
            pOutputVar->Set(m_pOutputTexture->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

        // Update shader parameters
        struct Params { float threshold; float softness; float pad[2]; };
        Params params = { threshold, softness, 0, 0 };
        pContext->UpdateBuffer(m_pCB, 0, sizeof(Params), &params, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_NONE);

        DispatchComputeAttribs Attribs;
        Attribs.ThreadGroupCountX = (OutputTexDesc.Width + 8 - 1) / 8;
        Attribs.ThreadGroupCountY = (OutputTexDesc.Height + 8 - 1) / 8;
        Attribs.ThreadGroupCountZ = 1;

        RefCntAutoPtr<IFence> m_pFence;
        FenceDesc fenceDesc;
        fenceDesc.Name = "ComputeFence";
        pDevice->CreateFence(fenceDesc, &m_pFence);

        pContext->DispatchCompute(Attribs);
        pContext->EnqueueSignal(m_pFence, 1);
        pContext->Flush();

        // Readback
        TextureDesc StagingTexDesc;
        StagingTexDesc.Name = "Staging Texture";
        StagingTexDesc.Width = OutputTexDesc.Width;
        StagingTexDesc.Height = OutputTexDesc.Height;
        StagingTexDesc.Type = RESOURCE_DIM_TEX_2D;
        StagingTexDesc.Format = OutputTexDesc.Format;
        StagingTexDesc.Usage = USAGE_STAGING;
        StagingTexDesc.BindFlags = BIND_NONE;
        StagingTexDesc.CPUAccessFlags = CPU_ACCESS_READ;

        RefCntAutoPtr<ITexture> pStagingTexture;
        pDevice->CreateTexture(StagingTexDesc, nullptr, &pStagingTexture);

        CopyTextureAttribs CopyAttribs;
        CopyAttribs.pSrcTexture = m_pOutputTexture;
        CopyAttribs.SrcMipLevel = 0;
        CopyAttribs.SrcSlice = 0;
        CopyAttribs.pDstTexture = pStagingTexture;
        CopyAttribs.DstMipLevel = 0;
        CopyAttribs.DstSlice = 0;
        CopyAttribs.DstX = 0;
        CopyAttribs.DstY = 0;
        CopyAttribs.DstZ = 0;

        StateTransitionDesc Barriers[1];
        Barriers[0].pResource = m_pOutputTexture;
        Barriers[0].OldState = RESOURCE_STATE_UNKNOWN;
        Barriers[0].NewState = RESOURCE_STATE_COPY_SOURCE;
        pContext->TransitionResourceStates(1, Barriers);
        pContext->CopyTexture(CopyAttribs);
        pContext->Flush();
        pContext->FinishFrame();

        MappedTextureSubresource MappedData;
        pContext->MapTextureSubresource(pStagingTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);
        if (MappedData.pData) {
            cv::Mat result(static_cast<int>(OutputTexDesc.Height),
                static_cast<int>(OutputTexDesc.Width),
                CV_32FC4, MappedData.pData, MappedData.Stride);
            result.copyTo(mat);
            pContext->UnmapTextureSubresource(pStagingTexture, 0, 0);
        }
    }
};

ThresholdCS::ThresholdCS(IRenderDevice* pDevice, IDeviceContext* pContext)
    : impl_(new Impl(pDevice, pContext)) {}
ThresholdCS::~ThresholdCS() { delete impl_; }

void ThresholdCS::loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename) {
    if (!baseDir.exists()) { qWarning() << "Base dir does not exist:" << baseDir.path(); return; }
    QString fullPath;
    QDirIterator it(baseDir.path(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString currentPath = it.next();
        QFileInfo fileInfo(currentPath);
        if (fileInfo.fileName() == filename) {
            fullPath = fileInfo.absoluteFilePath();
            break;
        }
    }
    if (fullPath.isEmpty()) { qWarning() << "Shader file" << filename << "not found in" << baseDir.path(); return; }
    impl_->readFromByteCode(fullPath);
}

void ThresholdCS::Process(cv::Mat& mat, float threshold, float softness) {
    impl_->Process(mat, threshold, softness);
}

}
