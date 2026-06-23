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

export module ImageProcessing.EmbossCS;

import Image;

export namespace ArtifactCore
{
 using namespace Diligent;

 class LIBRARY_DLL_API EmbossCS
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit EmbossCS(IRenderDevice* pDevice, IDeviceContext* pContext);
  ~EmbossCS();
  void loadShaderBinaryFile(const QString& path);
  void loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename);
  void Process(cv::Mat& mat, float intensity, float angle);
 };

}
