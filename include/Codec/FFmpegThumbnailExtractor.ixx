
module;
#include "../Define/DllExportMacro.hpp"
#include <QImage>
export module Codec.Thumbnail.FFmpeg;

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


 enum class ThumbnailExtractorMessage {
  Success,
  FileNotFound,
  NoVideoStream,
  DecoderNotFound,
  DecodeError,
  UnknownError

 };

 struct ThumbnailExtractorResult {
  QImage image;
  bool success = false;
  ThumbnailExtractorMessage message = ThumbnailExtractorMessage::UnknownError;
 };


 class LIBRARY_DLL_API FFmpegThumbnailExtractor {
 private:
  class Impl;
  Impl* impl_;
  FFmpegThumbnailExtractor(FFmpegThumbnailExtractor&&) noexcept;
  FFmpegThumbnailExtractor& operator=(FFmpegThumbnailExtractor&&) noexcept=default;
 public:
  FFmpegThumbnailExtractor();
  ~FFmpegThumbnailExtractor();
  FFmpegThumbnailExtractor(const FFmpegThumbnailExtractor&) = delete;
  FFmpegThumbnailExtractor& operator=(const FFmpegThumbnailExtractor&) = delete;
  QImage extractThumbnailOld(const QString& videoFilePath);
  QImage extractThumbnailAtTimestamp(const QString& videoPath, qint64 timestampMs);
  QImage extractEmbeddedThumbnail(const QString& videoPath);

  ThumbnailExtractorResult extractThumbnail(const UniString& str);

 };








};