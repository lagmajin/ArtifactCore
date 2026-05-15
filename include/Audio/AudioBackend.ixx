module;
#include <utility>
#include <memory>
#include <functional>
#include <QtCore/QString>

export module Audio.Backend;

export namespace ArtifactCore {

// Exported wrapper to keep QtMultimedia types out of the module interface.
struct AudioDeviceInfo {
  QString description;

  bool isValid() const {
    return !description.isEmpty();
  }
};

enum class AudioBackendSampleFormat {
  Float32,
  Int16
};

struct AudioBackendFormat {
  int sampleRate = 0;
  int channelCount = 0;
  AudioBackendSampleFormat sampleFormat = AudioBackendSampleFormat::Float32;

  bool isValid() const {
    return sampleRate > 0 && channelCount > 0;
  }
};

 using AudioCallback = std::function<void(float* buffer, int frames, int channels)>;

 export class AudioBackend {
 public:
  virtual ~AudioBackend() = default;

  // Connection lifecycle
  virtual bool open(const AudioDeviceInfo& device, const AudioBackendFormat& format) = 0;
  virtual void close() = 0;
  virtual void start(AudioCallback callback) = 0;
  virtual void stop() = 0;

  // Query helpers
  virtual bool isActive() const = 0;
  virtual AudioBackendFormat currentFormat() const = 0;
  virtual QString backendName() const = 0;
 };

};
