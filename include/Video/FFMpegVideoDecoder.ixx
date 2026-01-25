module;
#include "../Define/DllExportMacro.hpp"

#include <QString>
#include <QImage>
#include <QByteArray>

export module Codec.FFmpegVideoDecoder;

import std;

struct DecodedFrame {
 QImage image;
 int64_t pts; // stream time_base
};

//struct AVFrame;
export namespace ArtifactCore {
 enum class MediaType {
  Video,
  Audio,
  EndOfFile, // ストリームの終端
  None       // 何も得られなかった場合
 };

 struct MediaFrame {
  MediaType type = MediaType::None;
  double    timestamp = 0.0; // タイムスタンプ（秒）

  // ビデオフレームの場合
  QImage    videoImage;

  // オーディオフレームの場合
  QByteArray audioSamples; // 生のオーディオデータ (例: S16形式)
  int       audioChannels = 0;
  int       audioSampleRate = 0;
  // 必要に応じて、オーディオのサンプルフォーマット (AV_SAMPLE_FMT_S16など) も追加
 };



 class LIBRARY_DLL_API FFmpegVideoDecoder {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegVideoDecoder() noexcept;
  ~FFmpegVideoDecoder();
  bool openFile(const QString& path);
  void closeFile();
  QImage decodeNextVideoFrame();
  void flush();
 };

 void FFmpegVideoDecoder::flush()
 {

 }






};