module;
#include <limits>
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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Image.Raw;

namespace ArtifactCore {


 int RawImage::getPixelTypeSizeInBytes() const
 {
  if (pixelType == "uint8") return 1;
  if (pixelType == "int8") return 1;
  if (pixelType == "uint16") return 2;
  if (pixelType == "int16") return 2;
  if (pixelType == "half") return 2;
  if (pixelType == "uint32") return 4;
  if (pixelType == "int32") return 4;
  if (pixelType == "float") return 4;
  if (pixelType == "uint64") return 8;
  if (pixelType == "int64") return 8;
  if (pixelType == "double") return 8;

  // 未知のタイプの場合
  return 0;
 }

 bool RawImage::isValid() const
 {
  if (width <= 0 || height <= 0 || channels <= 0) return false;

  const int bytesPerChannel = getPixelTypeSizeInBytes();
  if (bytesPerChannel <= 0) return false;

  const qsizetype maximumSize = std::numeric_limits<qsizetype>::max();
  qsizetype requiredBytes = static_cast<qsizetype>(width);
  if (requiredBytes > maximumSize / height) return false;
  requiredBytes *= height;
  if (requiredBytes > maximumSize / channels) return false;
  requiredBytes *= channels;
  if (requiredBytes > maximumSize / bytesPerChannel) return false;
  requiredBytes *= bytesPerChannel;

  return data.size() >= requiredBytes;
 }



};
