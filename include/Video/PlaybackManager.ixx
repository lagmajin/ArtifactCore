module;
#include <QObject>
#include <wobjectdefs.h>

export module Video.PlaybackManager;

export namespace ArtifactCore {

 class PlaybackManager : public QObject
 {
  W_OBJECT(PlaybackManager)
 private:
  class Impl;
  Impl* impl_;
 public:

  explicit PlaybackManager(QObject* parent = nullptr);
  ~PlaybackManager();

  bool open(const QString& filePath);
  void play();
  void pause();
  void stop();
  static constexpr int VIDEO_QUEUE_SIZE_THRESHOLD = 5; // ビデオキューの最小サイズ
  static constexpr int AUDIO_QUEUE_SIZE_THRESHOLD = 20; // オーディオキューの最小サイズ (より多め)
  static constexpr double SYNC_THRESHOLD_MS = 100.0; // 同期の許容誤差ミリ秒
  static constexpr int PLAYBACK_INTERVAL_MS = 10;
 };









};