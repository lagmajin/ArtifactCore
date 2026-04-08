module;
#include "../../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <opencv2/opencv.hpp>
export module ImageProcessing.SpectralGlow;





export namespace ArtifactCore {


 class LIBRARY_DLL_API  SpectralGlow {
 private:

 public:
  SpectralGlow();
  ~SpectralGlow();
  void Process(cv::Mat& mat);
  void Process2(cv::Mat& mat);
  void Process3(cv::Mat& mat);
  void Process4(cv::Mat& mat);
  void ElegantGlow(cv::Mat& mat);
  void Process6(cv::Mat& mat);
 };










};
