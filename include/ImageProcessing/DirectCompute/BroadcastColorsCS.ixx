module;
#include <utility>

#include "../../Define/DllExportMacro.hpp"
#include <QDir>
#include <QString>

#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <opencv2/core/mat.hpp>

export module ImageProcessing.BroadcastColorsCS;

import Image;

export namespace ArtifactCore
{
 using namespace Diligent;

 class LIBRARY_DLL_API BroadcastColorsCS
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit BroadcastColorsCS(IRenderDevice* pDevice, IDeviceContext* pContext);
  ~BroadcastColorsCS();
  void loadShaderBinaryFile(const QString& path);
  void loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename);
  void Process(cv::Mat& mat, int standard, float reduceSaturation);
 };

}
