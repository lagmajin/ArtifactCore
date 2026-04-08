module;
//#include <memory>
#include "../Define/DllExportMacro.hpp"
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
#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#endif
export module ImageF32x4;






export namespace ArtifactCore {

 class LIBRARY_DLL_API ImageF32x4 {
 private:
  struct Impl;
  Impl* impl_;
  //std::unique_ptr<Impl> pimpl_;
 public:
  ImageF32x4();
  ImageF32x4(int width, int height);
  ~ImageF32x4();
  int width() const;
  int height() const;

 };




}