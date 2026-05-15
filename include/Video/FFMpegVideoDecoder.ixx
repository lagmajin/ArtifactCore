module;
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
#include <cstring>
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
#include <QString>
#include <QByteArray>
export module Codec.FFmpegVideoDecoder;

import Video.VideoFrame;

export namespace ArtifactCore {
 enum class MediaType {
  Video,
  Audio,
  EndOfFile,
  None
 };

 class LIBRARY_DLL_API FFmpegVideoDecoder {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegVideoDecoder() noexcept;
  ~FFmpegVideoDecoder();

  FFmpegVideoDecoder(const FFmpegVideoDecoder&) = delete;
  FFmpegVideoDecoder& operator=(const FFmpegVideoDecoder&) = delete;

  bool openFile(const QString& path);
  void closeFile();
  DecodedVideoFrame decodeNextVideoFrameRaw();
  void flush();
 };
}
