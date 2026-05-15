module;
class tst_QList;
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
#include <QList>
#include <QString>
export module Media.Encoder.FFmpegAudioDecoder;

import Utils.Size.Like;
import Utils.String.UniString;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API FFmpegAudioDecoder
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegAudioDecoder();
  ~FFmpegAudioDecoder();
  bool openFile(const QString& path);
  void closeFile();
  void seek(double seek);
  void fillCacheAsync(double start, double end);
  void flush();
  bool isSameFile(const UniString& path) const;
 };





};
