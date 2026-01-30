module;
#define QT_NO_KEYWORDS
#include <QString>
#include <QImage>
#include <QByteArray>

export module MediaPlaybackController;

import MediaSource;
import MediaReader;
import MediaImageFrameDecoder;
import MediaAudioDecoder;
import Media.MetaData;
import std;

export namespace ArtifactCore {

 // 再生状態
 enum class PlaybackState {
  Stopped,
  Playing,
  Paused,
  Buffering,
  Error
 };

 // シーク精度
 enum class SeekMode {
  Fast,       // 高速（キーフレームのみ）
  Accurate    // 正確（すべてのフレーム）
 };

 // 再生速度
 enum class PlaybackSpeed {
  QuarterSpeed,    // 0.25x
  HalfSpeed,       // 0.5x
  NormalSpeed,     // 1.0x
  DoubleSpeed,     // 2.0x
  QuadrupleSpeed   // 4.0x
 };

 // 再生情報
 struct PlaybackInfo {
  int64_t currentPositionMs = 0;     // 現在位置（ミリ秒）
  int64_t durationMs = 0;            // 総デュレーション（ミリ秒）
  double currentPositionSec = 0.0;   // 現在位置（秒）
  double durationSec = 0.0;          // 総デュレーション（秒）
  double percentageComplete = 0.0;   // 進捗率（0.0-100.0）
  int64_t currentFrame = 0;          // 現在のフレーム番号
  int64_t totalFrames = 0;           // 総フレーム数
  double fps = 0.0;                  // フレームレート
  PlaybackState state = PlaybackState::Stopped;
  double playbackSpeed = 1.0;        // 再生速度
  bool isMuted = false;              // ミュート状態
  float volume = 1.0f;               // 音量（0.0-1.0）
 };

 // 再生イベントのコールバック型
 using PlaybackStateChangedCallback = std::function<void(PlaybackState)>;
 using PositionChangedCallback = std::function<void(int64_t)>;
 using ErrorCallback = std::function<void(const QString&)>;
 using EndOfMediaCallback = std::function<void()>;

 class MediaPlaybackController {
 private:
  class Impl;
  Impl* impl_;

 public:
  MediaPlaybackController();
  ~MediaPlaybackController();

  // コピー/ムーブ
  MediaPlaybackController(const MediaPlaybackController&) = delete;
  MediaPlaybackController& operator=(const MediaPlaybackController&) = delete;
  MediaPlaybackController(MediaPlaybackController&&) noexcept;
  MediaPlaybackController& operator=(MediaPlaybackController&&) noexcept;

  // ---- メディア操作 ----

  // メディアを開く
  bool openMedia(const QString& url);
  
  // ファイルからメディアを開く
  bool openMediaFile(const QString& filePath);
  
  // メディアを閉じる
  void closeMedia();
  
  // メディアが開いているか
  bool isMediaOpen() const;

  // ---- 再生制御 ----

  // 再生
  void play();
  
  // 一時停止
  void pause();
  
  // 停止
  void stop();
  
  // 再生/一時停止トグル
  void togglePlayPause();

  // ---- シーク操作 ----

  // 指定位置にシーク（ミリ秒）
  void seek(int64_t timestampMs, SeekMode mode = SeekMode::Fast);
  
  // 指定位置にシーク（秒）
  void seekToSeconds(double seconds, SeekMode mode = SeekMode::Fast);
  
  // 指定フレームにシーク
  void seekToFrame(int64_t frameNumber);
  
  // 相対的にシーク（ミリ秒）
  void seekRelative(int64_t deltaMs);
  
  // 最初に戻る
  void seekToBeginning();
  
  // 最後に移動
  void seekToEnd();
  
  // 次のフレーム
  void stepForward();
  
  // 前のフレーム
  void stepBackward();

  // ---- 再生速度制御 ----

  // 再生速度を設定（0.25x～4.0x）
  void setPlaybackSpeed(double speed);
  void setPlaybackSpeed(PlaybackSpeed speed);
  double getPlaybackSpeed() const;

  // ---- オーディオ制御 ----

  // 音量を設定（0.0-1.0）
  void setVolume(float volume);
  float getVolume() const;
  
  // ミュート
  void setMuted(bool muted);
  bool isMuted() const;
  void toggleMute();

  // ---- フレーム取得 ----

  // 次のビデオフレームを取得
  QImage getNextVideoFrame();
  
  // 現在のビデオフレームを取得（再生位置を進めない）
  QImage getCurrentVideoFrame();
  
  // 指定位置のビデオフレームを取得
  QImage getVideoFrameAt(int64_t timestampMs);
  QImage getVideoFrameAtFrame(int64_t frameNumber);
  
  // 次のオーディオフレームを取得
  QByteArray getNextAudioFrame();

  // ---- 状態取得 ----

  // 再生状態
  PlaybackState getState() const;
  bool isPlaying() const;
  bool isPaused() const;
  bool isStopped() const;
  
  // 再生情報
  PlaybackInfo getPlaybackInfo() const;
  
  // メタデータ取得
  MediaMetaData getMetadata() const;
  
  // 現在位置（ミリ秒）
  int64_t getCurrentPosition() const;
  
  // 現在位置（秒）
  double getCurrentPositionSeconds() const;
  
  // デュレーション（ミリ秒）
  int64_t getDuration() const;
  
  // デュレーション（秒）
  double getDurationSeconds() const;
  
  // 現在のフレーム番号
  int64_t getCurrentFrame() const;
  
  // 総フレーム数
  int64_t getTotalFrames() const;
  
  // フレームレート
  double getFrameRate() const;
  
  // 進捗率（0.0-100.0）
  double getProgressPercentage() const;

  // ---- コールバック設定 ----

  // 再生状態変更コールバック
  void setStateChangedCallback(PlaybackStateChangedCallback callback);
  
  // 再生位置変更コールバック
  void setPositionChangedCallback(PositionChangedCallback callback);
  
  // エラーコールバック
  void setErrorCallback(ErrorCallback callback);
  
  // メディア終了コールバック
  void setEndOfMediaCallback(EndOfMediaCallback callback);

  // ---- ループ設定 ----

  // ループ再生を有効/無効
  void setLooping(bool enabled);
  bool isLooping() const;
  
  // ループ範囲を設定（ミリ秒）
  void setLoopRange(int64_t startMs, int64_t endMs);
  void clearLoopRange();

  // ---- その他 ----

  // バッファリング進捗（0.0-100.0）
  double getBufferingProgress() const;
  
  // 最後のエラーメッセージ
  QString getLastError() const;
  
  // サムネイルを生成
  QImage generateThumbnail(int64_t timestampMs, const QSize& size);
  
  // 複数のサムネイルを生成
  std::vector<QImage> generateThumbnails(int count, const QSize& size);
 };

} // namespace ArtifactCore