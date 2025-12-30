
module;
#include "../Define/DllExportMacro.hpp"
#include <QImage>
export module Codec.Thumbnail.FFmpeg;

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
  FFmpegThumbnailExtractor& operator=(FFmpegThumbnailExtractor&&) noexcept;
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