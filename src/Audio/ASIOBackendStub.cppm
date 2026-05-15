module;
#include <QString>
#include <QDebug>

module Audio.Backend.ASIOStub;

import std;
import Audio.Backend;
import Audio.Backend.WASAPI;

namespace ArtifactCore {

/**
 * @brief ASIOBackendStub の内部実装
 */
class ASIOBackendStub::Impl {
public:
    std::unique_ptr<WASAPIBackend> wasapi_;
    bool isOpen_ = false;
    AudioCallback callback_;
    
    Impl() : wasapi_(std::make_unique<WASAPIBackend>()) {}
};

ASIOBackendStub::ASIOBackendStub()
    : impl_(std::make_unique<Impl>())
{
}

ASIOBackendStub::~ASIOBackendStub() {
    close();
}

bool ASIOBackendStub::open(const AudioDeviceInfo& device,
                           const AudioBackendFormat& format)
{
    if (impl_->isOpen_) {
        qWarning() << "[ASIOBackendStub] Already open";
        return false;
    }
    
    // WASAPI に委譲
    if (!impl_->wasapi_->open(device, format)) {
        qWarning() << "[ASIOBackendStub] Failed to open WASAPI backend";
        return false;
    }
    
    impl_->isOpen_ = true;
    qDebug() << "[ASIOBackendStub] Opened successfully";
    return true;
}

void ASIOBackendStub::close() {
    if (!impl_->isOpen_) {
        return;
    }
    
    impl_->wasapi_->close();
    impl_->isOpen_ = false;
    impl_->callback_ = nullptr;
    
    qDebug() << "[ASIOBackendStub] Closed";
}

void ASIOBackendStub::start(AudioCallback callback) {
    if (!impl_->isOpen_) {
        qWarning() << "[ASIOBackendStub] start() called but not open";
        return;
    }
    
    impl_->callback_ = callback;
    impl_->wasapi_->start(callback);
    
    qDebug() << "[ASIOBackendStub] Started";
}

void ASIOBackendStub::stop() {
    if (!impl_->isOpen_) {
        return;
    }
    
    impl_->wasapi_->stop();
    
    qDebug() << "[ASIOBackendStub] Stopped";
}

bool ASIOBackendStub::isActive() const {
    return impl_->wasapi_->isActive();
}

AudioBackendFormat ASIOBackendStub::currentFormat() const {
    return impl_->wasapi_->currentFormat();
}

QString ASIOBackendStub::backendName() const {
    return QString::fromUtf8("ASIO(stub)");
}

} // namespace ArtifactCore
