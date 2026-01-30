module;
#define QT_NO_KEYWORDS
#include <QByteArray>
#include <QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

export module MediaAudioDecoder;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {

 // オーディオサンプルフォーマット
 enum class AudioSampleFormat {
  Unknown,
  Int16,          // 16-bit signed integer
  Int32,          // 32-bit signed integer
  Float,          // 32-bit float
  Double,         // 64-bit double
  Int16Planar,    // 16-bit signed integer (planar)
  Int32Planar,    // 32-bit signed integer (planar)
  FloatPlanar,    // 32-bit float (planar)
  DoublePlanar    // 64-bit double (planar)
 };

 // オーディオ情報
 struct AudioInfo {
  int sampleRate = 0;              // サンプルレート（Hz）
  int channels = 0;                // チャンネル数
  int64_t channelLayout = 0;       // チャンネルレイアウト
  AudioSampleFormat format = AudioSampleFormat::Unknown;
  UniString codecName;             // コーデック名
  int bitrate = 0;                 // ビットレート（bps）
  int bitsPerSample = 0;           // サンプルあたりのビット数
  int64_t totalSamples = 0;        // 総サンプル数
  double duration = 0.0;           // デュレーション（秒）
 };

 // デコード結果
 struct AudioDecodeResult {
  bool success = false;
  QByteArray data;                 // デコードされたPCMデータ
  int samplesDecoded = 0;          // デコードされたサンプル数
  int64_t timestamp = 0;           // タイムスタンプ（マイクロ秒）
  UniString errorMessage;
 };

 // リサンプリング設定
 struct ResamplingConfig {
  int targetSampleRate = 0;        // 目標サンプルレート（0=変換しない）
  int targetChannels = 0;          // 目標チャンネル数（0=変換しない）
  AudioSampleFormat targetFormat = AudioSampleFormat::Unknown;
  int64_t targetChannelLayout = 0;
 };

 // デコーダー統計情報
 struct DecoderStatistics {
  int64_t framesDecoded = 0;
  int64_t samplesDecoded = 0;
  int64_t bytesDecoded = 0;
  double totalDecodeTime = 0.0;    // 秒
  double averageDecodeTime = 0.0;  // フレームあたり（ミリ秒）
  int errors = 0;
 };

 class MediaAudioDecoder {
 private:
  class Impl;
  Impl* impl_;

 public:
  MediaAudioDecoder();
  ~MediaAudioDecoder();

  // コピー/ムーブ
  MediaAudioDecoder(const MediaAudioDecoder&) = delete;
  MediaAudioDecoder& operator=(const MediaAudioDecoder&) = delete;
  MediaAudioDecoder(MediaAudioDecoder&&) noexcept;
  MediaAudioDecoder& operator=(MediaAudioDecoder&&) noexcept;

  // ---- 初期化 ----

  // コーデックパラメータから初期化
  bool initialize(AVCodecParameters* codecParams);
  
  // コーデックコンテキストから初期化
  bool initialize(AVCodecContext* codecContext);
  
  // コーデック名から初期化
  bool initializeByCodecName(const UniString& codecName);
  
  // リセット（再初期化）
  void reset();
  
  // 初期化済みか
  bool isInitialized() const;

  // ---- デコード ----

  // パケットをデコード
  QByteArray decodeFrame(AVPacket* packet);
  
  // パケットをデコード（詳細な結果）
  AudioDecodeResult decodeFrameDetailed(AVPacket* packet);
  
  // 複数パケットをデコード
  std::vector<QByteArray> decodeFrames(const std::vector<AVPacket*>& packets);
  
  // バッファをフラッシュ
  void flush();
  
  // バッファに残っているフレームを取得
  QByteArray flushAndGetRemaining();

  // ---- リサンプリング ----

  // リサンプリングを有効化
  bool enableResampling(const ResamplingConfig& config);
  
  // リサンプリングを無効化
  void disableResampling();
  
  // リサンプリングが有効か
  bool isResamplingEnabled() const;
  
  // 現在のリサンプリング設定
  ResamplingConfig getResamplingConfig() const;

  // ---- オーディオ情報 ----

  // オーディオ情報を取得
  AudioInfo getAudioInfo() const;
  
  // サンプルレート
  int getSampleRate() const;
  
  // チャンネル数
  int getChannels() const;
  
  // チャンネルレイアウト
  int64_t getChannelLayout() const;
  
  // サンプルフォーマット
  AudioSampleFormat getSampleFormat() const;
  
  // コーデック名
  UniString getCodecName() const;
  
  // ビットレート
  int getBitrate() const;

  // ---- 統計情報 ----

  // 統計情報を取得
  DecoderStatistics getStatistics() const;
  
  // 統計情報をリセット
  void resetStatistics();
  
  // デコード済みフレーム数
  int64_t getDecodedFrameCount() const;
  
  // デコード済みサンプル数
  int64_t getDecodedSampleCount() const;

  // ---- エラーハンドリング ----

  // 最後のエラーメッセージ
  UniString getLastError() const;
  
  // エラーがあるか
  bool hasError() const;
  
  // エラーをクリア
  void clearError();

  // ---- ユーティリティ ----

  // サンプルフォーマットを文字列に変換
  static UniString sampleFormatToString(AudioSampleFormat format);
  
  // AVSampleFormatからAudioSampleFormatに変換
  static AudioSampleFormat fromAVSampleFormat(AVSampleFormat avFormat);
  
  // AudioSampleFormatからAVSampleFormatに変換
  static AVSampleFormat toAVSampleFormat(AudioSampleFormat format);
  
  // チャンネルレイアウトを文字列に変換
  static UniString channelLayoutToString(int64_t layout);
  
  // サンプル数からバイト数を計算
  static int64_t samplesToBytes(int64_t samples, int channels, AudioSampleFormat format);
  
  // バイト数からサンプル数を計算
  static int64_t bytesToSamples(int64_t bytes, int channels, AudioSampleFormat format);

  // ---- FFmpeg直接アクセス（上級者向け） ----

  // 内部のAVCodecContextを取得
  AVCodecContext* getCodecContext() const;
  
  // 内部のSwrContextを取得
  SwrContext* getSwrContext() const;
 };

} // namespace ArtifactCore