module;
#include <QtMultimedia/QAudioDevice>
#include <memory>
#include <functional>

export module Audio.Backend.ASIOStub;

import Audio.Backend;

export namespace ArtifactCore {

/**
 * @brief ASIO バックエンドスタブ
 * 
 * ASIO SDK を使用しないスタブ実装。
 * 内部では WASAPI を使用して互換性を確保。
 */
export class ASIOBackendStub : public AudioBackend {
public:
  ASIOBackendStub();
  ~ASIOBackendStub() override;

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
