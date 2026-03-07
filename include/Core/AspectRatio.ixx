module;
#include <memory>
#include <QString>
#include "../Define/DllExportMacro.hpp"

export module Core.AspectRatio;

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



import Utils.String.UniString;

export namespace ArtifactCore {

 class LIBRARY_DLL_API AspectRatio {
 public:
  AspectRatio();
  AspectRatio(int width, int height);
  ~AspectRatio();

  // Copy and Move operations
  AspectRatio(const AspectRatio& other);
  AspectRatio& operator=(const AspectRatio& other);
  AspectRatio(AspectRatio&& other) noexcept;
  AspectRatio& operator=(AspectRatio&& other) noexcept;

  double ratio() const;
  UniString toString() const;
  void setFromString(const UniString& str);
  void simplify();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
 };

}