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

  FFmpegVideoDecoder(FFmpegVideoDecoder&& other) noexcept;
  FFmpegVideoDecoder& operator=(FFmpegVideoDecoder&& other) noexcept;

  bool openFile(const QString& path);
  void closeFile();
  DecodedVideoFrame decodeNextVideoFrameRaw();
  // Frame-number seek (uses the video stream r_frame_rate). Returns false
  // when no stream is open or the stream has no frame rate information.
  bool seekToFrame(int64_t frameNumber);
  // Exact-frame decode: seeks backward to the nearest keyframe, then decodes
  // forward until the presented PTS reaches the requested frame's timestamp.
  // Returns std::monostate when the target PTS cannot be reached (bad stream
  // metadata, seek failure, or EOF before the target).
  DecodedVideoFrame decodeFrameAtRaw(int64_t frameNumber);
  void flush();

  // Stream metadata. Valid after a successful openFile(); 0 otherwise.
  int width() const;
  int height() const;
  double fps() const;
 };
}
