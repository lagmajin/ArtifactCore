module;
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtCore/QString>
#include <memory>
#include <functional>
export module Audio.Backend;

export namespace ArtifactCore {

 using AudioCallback = std::function<void(float* buffer, int frames, int channels)>;

 export class AudioBackend {
 public:
  virtual ~AudioBackend() = default;

  // Connection lifecycle
  virtual bool open(const QAudioDevice& device, const QAudioFormat& format) = 0;
  virtual void close() = 0;
  virtual void start(AudioCallback callback) = 0;
  virtual void stop() = 0;

  // Query helpers
  virtual bool isActive() const = 0;
  virtual QAudioFormat currentFormat() const = 0;
  virtual QString backendName() const = 0;
 };

};
