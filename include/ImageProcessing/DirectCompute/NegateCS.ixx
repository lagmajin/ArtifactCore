
module;


#include "../../Define/DllExportMacro.hpp"
#include <QDir>
#include <QString>

#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <opencv2/core/mat.hpp>
export module ImageProcessing.NegateCS;

import std;
import Image;

export namespace ArtifactCore
{
 using namespace Diligent;

 class LIBRARY_DLL_API NegateCS
 {
 private:
  class Impl;
  Impl* impl_;
 public:

  explicit NegateCS(RefCntAutoPtr<IRenderDevice> pDevice,RefCntAutoPtr<IDeviceContext> pContext);
  ~NegateCS();
  void loadShaderBinaryFile(const QString&path);
  void loadShaderBinaryFromDirectory(const QDir& baseDir,const QString& filename);
  void Process();
  void Process(cv::Mat& mat);
 };











};