module;
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>

#include <memory>
#include <functional>

export module Audio.Backend.WASAPI;

import Audio.Backend;

export namespace ArtifactCore {

export class WASAPIBackend : public AudioBackend {
public:
  WASAPIBackend();
  ~WASAPIBackend() override;

  bool open(const QAudioDevice& device, const QAudioFormat& format) override;
  void close() override;
  void start(AudioCallback callback) override;
  void stop() override;

  bool isActive() const override;
  QAudioFormat currentFormat() const override;
  QString backendName() const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore

