module;
#include <utility>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Audioclient.h>
#include <ksmedia.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <QString>
#include <thread>

module Audio.Backend.WASAPI;

import std;

namespace ArtifactCore {

class WASAPIBackend::Impl {
public:
  std::atomic<bool> active{false};
  std::atomic<bool> stopRequested{false};
  std::atomic<bool> stopJoined{false};
  AudioCallback callback;
  std::thread renderThread;
  AudioBackendFormat currentFormat;
  QString deviceName;

  IAudioClient* audioClient = nullptr;
  IAudioRenderClient* renderClient = nullptr;
  IMMDevice* device = nullptr;
  WAVEFORMATEX* mixFormat = nullptr;
  REFERENCE_TIME bufferDuration = 0;
  UINT32 bufferFrameCount = 0;
  bool mixFormatIsFloat = true;
  int mixChannels = 2;
  int mixSampleRate = 48000;
  bool comInitialized = false;
  HANDLE audioEvent = nullptr;
  HANDLE stopEvent = nullptr;

  ~Impl() {
    stopThread();
    releaseCom();
  }

  void stopThread() {
    stopRequested = true;
    if (stopEvent) {
      SetEvent(stopEvent);
    }
    if (audioEvent) {
      SetEvent(audioEvent);
    }
    if (renderThread.joinable() && !stopJoined.exchange(true)) {
      if (renderThread.get_id() == std::this_thread::get_id()) {
        renderThread.detach();
      } else {
        renderThread.join();
      }
    }
    active = false;
  }

  void releaseCom() {
    if (renderClient) { renderClient->Release(); renderClient = nullptr; }
    if (audioClient) { audioClient->Release(); audioClient = nullptr; }
    if (device) { device->Release(); device = nullptr; }
    if (mixFormat) { CoTaskMemFree(mixFormat); mixFormat = nullptr; }
    if (audioEvent) { CloseHandle(audioEvent); audioEvent = nullptr; }
    if (stopEvent) { CloseHandle(stopEvent); stopEvent = nullptr; }
    if (comInitialized) {
      CoUninitialize();
      comInitialized = false;
    }
  }

  void renderLoop() {
    if (!audioClient || !renderClient) {
      return;
    }

    const auto pump = [this]() {
      if (!callback) {
        return;
      }

      UINT32 padding = 0;
      if (FAILED(audioClient->GetCurrentPadding(&padding))) {
        return;
      }
      if (padding >= bufferFrameCount) {
        return;
      }

      const UINT32 framesToWrite = bufferFrameCount - padding;
      BYTE* data = nullptr;
      if (FAILED(renderClient->GetBuffer(framesToWrite, &data)) || !data) {
        return;
      }

      const int channels = std::max(1, mixChannels);
      const size_t sampleCount = static_cast<size_t>(framesToWrite) * static_cast<size_t>(channels);

      if (mixFormatIsFloat) {
        auto* out = reinterpret_cast<float*>(data);
        callback(out, static_cast<int>(framesToWrite), channels);
      } else {
        std::vector<float> temp(sampleCount, 0.0f);
        callback(temp.data(), static_cast<int>(framesToWrite), channels);
        auto* out = reinterpret_cast<std::int16_t*>(data);
        for (size_t i = 0; i < sampleCount; ++i) {
          const float sample = std::clamp(temp[i], -1.0f, 1.0f);
          out[i] = static_cast<std::int16_t>(std::lround(sample * 32767.0f));
        }
      }

      renderClient->ReleaseBuffer(framesToWrite, 0);
    };

    pump();
    while (!stopRequested) {
      HANDLE waits[2] = { stopEvent, audioEvent };
      const DWORD waitResult = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
      if (waitResult == WAIT_OBJECT_0) {
        break;
      }
      if (waitResult == WAIT_OBJECT_0 + 1) {
        pump();
      }
    }
  }
};

WASAPIBackend::WASAPIBackend() : impl_(std::make_unique<Impl>()) {}
WASAPIBackend::~WASAPIBackend() { close(); }

bool WASAPIBackend::open(const AudioDeviceInfo& device, const AudioBackendFormat& format) {
  close();

  impl_->deviceName = device.description;
  impl_->currentFormat = format;

  const HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
    return false;
  }
  impl_->comInitialized = (initHr == S_OK || initHr == S_FALSE);

  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&enumerator));
  if (FAILED(hr) || !enumerator) {
    impl_->releaseCom();
    return false;
  }

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device);
  enumerator->Release();
  if (FAILED(hr) || !impl_->device) {
    impl_->releaseCom();
    return false;
  }

  hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(&impl_->audioClient));
  if (FAILED(hr) || !impl_->audioClient) {
    impl_->releaseCom();
    return false;
  }

  hr = impl_->audioClient->GetMixFormat(&impl_->mixFormat);
  if (FAILED(hr) || !impl_->mixFormat) {
    impl_->releaseCom();
    return false;
  }

  impl_->mixChannels = std::max<UINT32>(1, impl_->mixFormat->nChannels);
  impl_->mixSampleRate = static_cast<int>(impl_->mixFormat->nSamplesPerSec);

  impl_->mixFormatIsFloat = false;
  if (impl_->mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    impl_->mixFormatIsFloat = true;
  } else if (impl_->mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(impl_->mixFormat);
    impl_->mixFormatIsFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
  }

  // Request a 50 ms shared-mode buffer (2400 frames at 48 kHz).
  // A larger buffer gives the render thread more headroom before an underflow
  // occurs if the OS scheduler delays the poll loop.
  impl_->bufferDuration = static_cast<REFERENCE_TIME>(
      (10000000LL * 2400) / std::max(1, impl_->mixSampleRate));
  hr = impl_->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, impl_->bufferDuration,
                                      0, impl_->mixFormat, nullptr);
  if (FAILED(hr)) {
    impl_->releaseCom();
    return false;
  }

  hr = impl_->audioClient->GetBufferSize(&impl_->bufferFrameCount);
  if (FAILED(hr) || impl_->bufferFrameCount == 0) {
    impl_->releaseCom();
    return false;
  }

  hr = impl_->audioClient->GetService(__uuidof(IAudioRenderClient),
                                      reinterpret_cast<void**>(&impl_->renderClient));
  if (FAILED(hr) || !impl_->renderClient) {
    impl_->releaseCom();
    return false;
  }

  impl_->audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  impl_->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!impl_->audioEvent || !impl_->stopEvent) {
    impl_->releaseCom();
    return false;
  }

  hr = impl_->audioClient->SetEventHandle(impl_->audioEvent);
  if (FAILED(hr)) {
    impl_->releaseCom();
    return false;
  }

  impl_->currentFormat.sampleRate = impl_->mixSampleRate;
  impl_->currentFormat.channelCount = impl_->mixChannels;
  impl_->currentFormat.sampleFormat = impl_->mixFormatIsFloat
      ? AudioBackendSampleFormat::Float32
      : AudioBackendSampleFormat::Int16;
  return true;
}

void WASAPIBackend::close() {
  if (!impl_) {
    return;
  }
  stop();
  impl_->releaseCom();
}

void WASAPIBackend::start(AudioCallback callback) {
  if (!impl_ || !impl_->audioClient || !impl_->renderClient) {
    return;
  }

  impl_->callback = std::move(callback);
  impl_->stopRequested = false;
  impl_->stopJoined = false;
  impl_->active = true;
  if (FAILED(impl_->audioClient->Start())) {
    impl_->active = false;
    return;
  }

  impl_->renderThread = std::thread([this]() {
    impl_->renderLoop();
  });
}

void WASAPIBackend::stop() {
  if (!impl_) {
    return;
  }

  if (!impl_->active.load() && !impl_->renderThread.joinable()) {
    return;
  }
  impl_->stopRequested = true;
  if (impl_->audioClient) {
    impl_->audioClient->Stop();
  }
  impl_->stopThread();
  impl_->active = false;
}

bool WASAPIBackend::isActive() const {
  return impl_ ? impl_->active.load() : false;
}

AudioBackendFormat WASAPIBackend::currentFormat() const {
  return impl_ ? impl_->currentFormat : AudioBackendFormat{};
}

QString WASAPIBackend::backendName() const {
  return QString::fromUtf8("WASAPI(shared)");
}

} // namespace ArtifactCore
