module;
#include <utility>

#include "../../Define/DllExportMacro.hpp"
#include <QDir>
#include <QString>

#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>

#include <iostream>
#include <vector>
#include <string>
//#include <map>
#include <unordered_map>
#include <set>
//#include <unordered_set>
//#include <memory>
//#include <algorithm>
//#include <cmath>
//#include <functional>
//#include <optional>
//#include <utility>
//#include <array>
//#include <chrono>
#include <filesystem>
//#include <fstream>
//#include <sstream>
//#include <stdexcept>
//#include <type_traits>
//#include <variant>
//#include <any>
//#include <atomic>
//#include <queue>
//#include <deque>
//#include <list>
//#include <tuple>
//#include <numeric>
//#include <regex>
//#include <random>
#include <opencv2/core/mat.hpp>
export module ImageProcessing.NegateCS;

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

  explicit NegateCS(IRenderDevice* pDevice, IDeviceContext* pContext);
  ~NegateCS();
  void loadShaderBinaryFile(const QString&path);
  void loadShaderBinaryFromDirectory(const QDir& baseDir,const QString& filename);
  void Process();
  void Process(cv::Mat& mat);
 };











};
