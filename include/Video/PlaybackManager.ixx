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
  static constexpr int VIDEO_QUEUE_SIZE_THRESHOLD = 5; // �r�f�I�L���[�̍ŏ��T�C�Y
  static constexpr int AUDIO_QUEUE_SIZE_THRESHOLD = 20; // �I�[�f�B�I�L���[�̍ŏ��T�C�Y (��葽��)
  static constexpr double SYNC_THRESHOLD_MS = 100.0; // �����̋��e�덷�~���b
  static constexpr int PLAYBACK_INTERVAL_MS = 10;
 };









};