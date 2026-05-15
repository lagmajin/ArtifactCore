module;
#include <utility>

#include <QString>
export module Media.Encoder.FFmpegAudioEncoder;

export namespace ArtifactCore
{

 class FFmpegAudioEncoder
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFmpegAudioEncoder();
  ~FFmpegAudioEncoder();

  FFmpegAudioEncoder(const FFmpegAudioEncoder&) = delete;
  FFmpegAudioEncoder& operator=(const FFmpegAudioEncoder&) = delete;

  FFmpegAudioEncoder(FFmpegAudioEncoder&& other) noexcept;
  FFmpegAudioEncoder& operator=(FFmpegAudioEncoder&& other) noexcept;

  // 音声ファイルをデコードしてビデオコンテナにmux
  // videoPath: 既にエンコード済みのビデオファイル
  // audioPath: 音声ソースファイル (WAV/MP3/AAC/FLAC/OGG等)
  // outputPath: 出力先ファイルパス (音声付きビデオ)
  // audioCodec: 出力音声コーデック ("aac", "mp3", "opus", "flac" — 空ならコンテナ既定)
  // audioBitrate: 音声ビットレート (bps, 0ならコーデック既定)
  static bool muxAudioWithVideo(
   const QString& videoPath,
   const QString& audioPath,
   const QString& outputPath,
   const QString& audioCodec = QString(),
   int audioBitrate = 0);

  // 音声ファイルを単独エンコード
  // inputPath: 音声ソースファイル
  // outputPath: 出力先ファイルパス
  // codec: 出力コーデック ("aac", "mp3", "opus", "flac")
  // bitrate: ビットレート (bps, 0ならコーデック既定)
  // sampleRate: サンプルレート (0ならソース維持)
  static bool encodeAudio(
   const QString& inputPath,
   const QString& outputPath,
   const QString& codec = "aac",
   int bitrate = 128000,
   int sampleRate = 0);

  // 最後のエラーメッセージ取得
  QString lastError() const;

  // 音声ファイルの情報を取得
  struct AudioInfo {
   bool valid = false;
   QString codec;
   int sampleRate = 0;
   int channels = 0;
   int64_t durationMs = 0;
   int bitrate = 0;
   QString errorMessage;
  };
  static AudioInfo probeAudioFile(const QString& filePath);
 };

}
