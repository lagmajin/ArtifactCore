module;
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <wobjectimpl.h>
#include <QObject>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QString>
#include <QByteArray>
#include <QHostAddress>

module Control.OSC.Input;

namespace ArtifactCore {

// ─────────────────────────────────────────────────────────
// OSC 1.0 最小パーサー
// ─────────────────────────────────────────────────────────

namespace {

// 文字列を4バイト境界にパディング
int paddedSize(int rawSize) {
    return (rawSize + 4) & ~3;
}

// ビッグエンディアン 32bit float → ホストエンディアン
float readBigEndianFloat(const uint8_t* data) {
    uint32_t raw = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8)  |
                   (static_cast<uint32_t>(data[3]));
    float result;
    std::memcpy(&result, &raw, sizeof(result));
    return result;
}

// ビッグエンディアン 32bit int → ホストエンディアン
int32_t readBigEndianInt32(const uint8_t* data) {
    return (static_cast<int32_t>(data[0]) << 24) |
           (static_cast<int32_t>(data[1]) << 16) |
           (static_cast<int32_t>(data[2]) << 8)  |
           (static_cast<int32_t>(data[3]));
}

// メッセージのパース結果
struct OscMessage {
    QString address;
    float value = 0.0f;
    bool valid = false;
};

OscMessage parseOscMessage(const QByteArray& data) {
    OscMessage msg;
    if (data.size() < 8) return msg;

    const auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());

    // OSC アドレス文字列を読む ('/' で開始)
    if (bytes[0] != '/') return msg;

    // アドレス文字列の長さを探す
    int addrLen = 0;
    for (int i = 0; i < data.size(); ++i) {
        if (bytes[i] == 0) { addrLen = i; break; }
    }
    if (addrLen == 0) return msg;

    msg.address = QString::fromUtf8(reinterpret_cast<const char*>(bytes), addrLen);

    // アドレスの後はパディング
    int offset = paddedSize(addrLen + 1);

    // タイプタグ (',' で開始)
    if (offset + 1 >= data.size() || bytes[offset] != ',') return msg;
    int tagLen = 0;
    for (int i = offset; i < data.size(); ++i) {
        if (bytes[i] == 0) { tagLen = i - offset; break; }
    }
    if (tagLen < 2) return msg; // ',' + 最低1文字

    char tag = static_cast<char>(bytes[offset + 1]);
    offset = paddedSize(offset + tagLen + 1);

    // 値を読む
    if (tag == 'f' && offset + 4 <= data.size()) {
        msg.value = readBigEndianFloat(bytes + offset);
        msg.valid = true;
    } else if (tag == 'i' && offset + 4 <= data.size()) {
        msg.value = static_cast<float>(readBigEndianInt32(bytes + offset));
        msg.valid = true;
    }

    return msg;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────
// OscInput::Impl
// ─────────────────────────────────────────────────────────
class OscInput::Impl {
public:
    QUdpSocket* socket = nullptr;
    uint16_t port_ = 0;
    bool running_ = false;

    void onReadyRead(OscInput* self) {
        if (!socket) return;
        while (socket->hasPendingDatagrams()) {
            QNetworkDatagram dg = socket->receiveDatagram();
            OscMessage msg = parseOscMessage(dg.data());
            if (msg.valid) {
                Q_EMIT self->messageReceived(msg.address, msg.value);
            }
        }
    }
};

W_OBJECT_IMPL(OscInput)

OscInput::OscInput(QObject* parent)
    : QObject(parent), impl_(new Impl()) {}

OscInput::~OscInput() {
    stopServer();
    delete impl_;
}

bool OscInput::startServer(uint16_t port) {
    if (impl_->running_) stopServer();

    auto* socket = new QUdpSocket(this);
    if (!socket->bind(QHostAddress::Any, port)) {
        delete socket;
        return false;
    }

    impl_->socket = socket;
    impl_->port_ = port;
    impl_->running_ = true;

    QObject::connect(socket, &QUdpSocket::readyRead, this, [this]() {
        impl_->onReadyRead(this);
    });

    return true;
}

void OscInput::stopServer() {
    if (impl_->socket) {
        impl_->socket->close();
        impl_->socket->deleteLater();
        impl_->socket = nullptr;
    }
    impl_->running_ = false;
    impl_->port_ = 0;
}

bool OscInput::isRunning() const {
    return impl_->running_;
}

uint16_t OscInput::port() const {
    return impl_->port_;
}

} // namespace ArtifactCore
