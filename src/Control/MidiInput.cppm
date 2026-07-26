module;
#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>

#include <wobjectimpl.h>
#include <QTimer>
#include <QObject>

module Control.Midi.Input;

namespace ArtifactCore {

// ─────────────────────────────────────────────────────────
// WinMM MIDI コールバック
// ─────────────────────────────────────────────────────────
#ifdef _WIN32
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                         DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    auto* self = reinterpret_cast<MidiInput::Impl*>(dwInstance);
    if (!self) return;

    if (wMsg == MIM_DATA) {
        uint8_t status  = static_cast<uint8_t>(dwParam1 & 0xFF);
        uint8_t data1   = static_cast<uint8_t>((dwParam1 >> 8) & 0xFF);
        uint8_t data2   = static_cast<uint8_t>((dwParam1 >> 16) & 0xFF);
        uint8_t channel = status & 0x0F;
        uint8_t msgType = status & 0xF0;

        if (msgType == 0xB0) {
            // Control Change
            self->enqueue([self, channel, data1, data2]() {
                if (self->owner)
                    Q_EMIT self->owner->ccReceived(channel, data1, data2);
            });
        } else if (msgType == 0x90 && data2 > 0) {
            // Note On
            self->enqueue([self, channel, data1, data2]() {
                if (self->owner)
                    Q_EMIT self->owner->noteOnReceived(channel, data1, data2);
            });
        } else if (msgType == 0x80 || (msgType == 0x90 && data2 == 0)) {
            // Note Off (or velocity=0 treated as note off)
            // For now we don't emit note off — can be added if needed
        }
    }
}
#endif

// ─────────────────────────────────────────────────────────
// MidiInput::Impl
// ─────────────────────────────────────────────────────────
class MidiInput::Impl {
public:
    MidiInput* owner = nullptr;
    QTimer flushTimer;
#ifdef _WIN32
    HMIDIIN hMidiIn_ = nullptr;
#endif
    uint32_t deviceId_ = 0;
    bool open_ = false;
    std::mutex queueMutex_;
    std::vector<std::function<void()>> pending_;

    void enqueue(std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pending_.push_back(std::move(fn));
        }
    }

    void flushPending() {
        std::vector<std::function<void()>> copy;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            copy.swap(pending_);
        }
        for (auto& fn : copy) fn();
    }
};

W_OBJECT_IMPL(MidiInput)

MidiInput::MidiInput(QObject* parent)
    : QObject(parent), impl_(new Impl())
{
    impl_->owner = this;
    impl_->flushTimer.setInterval(16); // ~60fps polling for MIDI callback
    impl_->flushTimer.setSingleShot(false);
    QObject::connect(&impl_->flushTimer, &QTimer::timeout, this, [this]() {
        impl_->flushPending();
    });
    impl_->flushTimer.start();
}

MidiInput::~MidiInput() {
    closeDevice();
    delete impl_;
}

std::vector<MidiInput::DeviceInfo> MidiInput::enumerateDevices() {
    std::vector<DeviceInfo> devices;
#ifdef _WIN32
    UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIINCAPSW caps;
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            DeviceInfo info;
            info.id = i;
            int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::vector<char> buf(static_cast<size_t>(len));
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, buf.data(), len, nullptr, nullptr);
                info.name = buf.data();
            }
            info.maxChannels = 16;
            info.isAvailable = true;
            devices.push_back(std::move(info));
        }
    }
#else
    (void)devices;
#endif
    return devices;
}

bool MidiInput::openDevice(uint32_t deviceId) {
    if (impl_->open_) closeDevice();

#ifdef _WIN32
    MMRESULT res = midiInOpen(&impl_->hMidiIn_, deviceId,
                               reinterpret_cast<DWORD_PTR>(MidiInProc),
                               reinterpret_cast<DWORD_PTR>(impl_.get()),
                               CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) return false;

    res = midiInStart(impl_->hMidiIn_);
    if (res != MMSYSERR_NOERROR) {
        midiInClose(impl_->hMidiIn_);
        impl_->hMidiIn_ = nullptr;
        return false;
    }

    impl_->deviceId_ = deviceId;
    impl_->open_ = true;
    return true;
#else
    (void)deviceId;
    return false;
#endif
}

void MidiInput::closeDevice() {
    if (!impl_->open_) return;
#ifdef _WIN32
    if (impl_->hMidiIn_) {
        midiInStop(impl_->hMidiIn_);
        midiInClose(impl_->hMidiIn_);
        impl_->hMidiIn_ = nullptr;
    }
#endif
    impl_->open_ = false;
}

int32_t MidiInput::activeDeviceId() const {
    return impl_->open_ ? static_cast<int32_t>(impl_->deviceId_) : -1;
}

bool MidiInput::isOpen() const {
    return impl_->open_;
}

} // namespace ArtifactCore
