
module;
#include "../Define/DllExportMacro.hpp"
#include <QImage>
export module Codec.Thubnail.FFmpeg;



export namespace ArtifactCore {


 class LIBRARY_DLL_API FFmpegThumbnailExtractor {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegThumbnailExtractor();
  ~FFmpegThumbnailExtractor();
  FFmpegThumbnailExtractor(const FFmpegThumbnailExtractor&) = delete;
  FFmpegThumbnailExtractor& operator=(const FFmpegThumbnailExtractor&) = delete;
  QImage extractThumbnail(const QString& videoFilePath);
  QImage extractThumbnailAtTimestamp(const QString& videoPath, qint64 timestampMs);
  QImage extractEmbeddedThumbnail(const QString& videoPath);
  // ムーブコンストラクタとムーブ代入演算子の追加 (現代的なC++では推奨)
  FFmpegThumbnailExtractor(FFmpegThumbnailExtractor&&) noexcept;
  FFmpegThumbnailExtractor& operator=(FFmpegThumbnailExtractor&&) noexcept;

  


 };








};