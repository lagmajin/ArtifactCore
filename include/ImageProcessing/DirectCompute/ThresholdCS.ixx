module;
#include <utility>
#include "../../Define/DllExportMacro.hpp"
#include <QDir>
#include <QString>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <vector>
#include <string>
#include <filesystem>
#include <opencv2/core/mat.hpp>

export module ImageProcessing.ThresholdCS;

import Image;

export namespace ArtifactCore {
using namespace Diligent;

class LIBRARY_DLL_API ThresholdCS {
private:
    class Impl;
    Impl* impl_;
public:
    explicit ThresholdCS(IRenderDevice* pDevice, IDeviceContext* pContext);
    ~ThresholdCS();
    void loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename);
    void Process(cv::Mat& mat, float threshold, float softness);
};

}
