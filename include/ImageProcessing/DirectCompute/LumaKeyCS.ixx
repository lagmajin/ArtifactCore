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

export module ImageProcessing.LumaKeyCS;

import Image;

export namespace ArtifactCore {
using namespace Diligent;

class LIBRARY_DLL_API LumaKeyCS {
private:
    class Impl;
    Impl* impl_;
public:
    explicit LumaKeyCS(IRenderDevice* pDevice, IDeviceContext* pContext);
    ~LumaKeyCS();
    void loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename);
    void Process(cv::Mat& mat, float lowThresh, float highThresh, float softness);
};

}
