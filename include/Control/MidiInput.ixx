module;
#include <cstdint>
#include <functional>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>
#include <wobjectdefs.h>

export module Control.Midi.Input;

import Core.ArtifactString;

export namespace ArtifactCore {

/**
 * @brief MIDI 入力デバイス管理
 *
 * Windows WinMM API (midiInOpen/midiInStart) を使用。
 * 受信した CC メッセージを ExternalControlManager::observeInput() にルーティングする。
 */
class MidiInput : public QObject {
    W_OBJECT(MidiInput)

public:
    struct DeviceInfo {
        uint32_t id = 0;
        String name;
        uint32_t maxChannels = 0;
        bool isAvailable = false;
    };

    explicit MidiInput(QObject* parent = nullptr);
    ~MidiInput() override;

    /// 利用可能な MIDI 入力デバイスを列挙
    static std::vector<DeviceInfo> enumerateDevices();

    /// デバイスを開く
    bool openDevice(uint32_t deviceId);

    /// デバイスを閉じる
    void closeDevice();

    /// 現在開いているデバイスID (-1 = none)
    int32_t activeDeviceId() const;

    /// デバイスが開いているか
    bool isOpen() const;

    // WinMM callback implementation needs to refer to the opaque state type.
    class Impl;

    /// CC 値が到着したときのシグナル
    void ccReceived(int channel, int controller, int value)
        W_SIGNAL(ccReceived, channel, controller, value);

    /// Note On が到着したときのシグナル
    void noteOnReceived(int channel, int note, int velocity)
        W_SIGNAL(noteOnReceived, channel, note, velocity);

private:
    Impl* impl_;
};

} // namespace ArtifactCore
