module;
#include <utility>
#include <memory>
#include <functional>

export module Audio.Backend.WASAPI;

import Audio.Backend;

export namespace ArtifactCore {

export class WASAPIBackend : public AudioBackend {
public:
  WASAPIBackend();
  ~WASAPIBackend() override;

  bool open(const AudioDeviceInfo& device, const AudioBackendFormat& format) override;
  void close() override;
  void start(AudioCallback callback) override;
  void stop() override;

  bool isActive() const override;
  AudioBackendFormat currentFormat() const override;
  QString backendName() const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore

