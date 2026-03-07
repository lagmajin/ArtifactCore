module;
#include <QList>
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
  return width > 0 && height > 0 && channels > 0 && !data.isEmpty() && !pixelType.isEmpty();
 }



};