module;
#include <utility>
#include <stdint.h>
#include <QtGui/QTransform>

module Transform._2D;

namespace ArtifactCore {

struct StaticTransform2D::Impl {
    float x = 0.0f;
    float y = 0.0f;
    float initialX = 1.0f;
    float initialY = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    float anchorPointX = 0.0f;
    float anchorPointY = 0.0f;

    Impl() = default;
    Impl(const Impl& other) = default;
};

StaticTransform2D::StaticTransform2D() : impl_(new Impl()) {}

StaticTransform2D::StaticTransform2D(const StaticTransform2D& other) 
    : impl_(new Impl(*other.impl_)) {}

StaticTransform2D::StaticTransform2D(StaticTransform2D&& other) noexcept 
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

StaticTransform2D::~StaticTransform2D() {
    delete impl_;
}

StaticTransform2D& StaticTransform2D::operator=(const StaticTransform2D& other) {
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

StaticTransform2D& StaticTransform2D::operator=(StaticTransform2D&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

float StaticTransform2D::scaleX() const { return impl_ ? impl_->scaleX : 1.0f; }
float StaticTransform2D::scaleY() const { return impl_ ? impl_->scaleY : 1.0f; }

void StaticTransform2D::setX(float x) { if (impl_) impl_->x = x; }
void StaticTransform2D::setY(float y) { if (impl_) impl_->y = y; }

void StaticTransform2D::setScaleX(float x) { if (impl_) impl_->scaleX = x; }
void StaticTransform2D::setScaleY(float y) { if (impl_) impl_->scaleY = y; }

float StaticTransform2D::x() const { return impl_ ? impl_->x : 0.0f; }
float StaticTransform2D::y() const { return impl_ ? impl_->y : 0.0f; }
float StaticTransform2D::rotation() const { return impl_ ? impl_->rotation : 0.0f; }

void StaticTransform2D::setInitialScaleX(float x) { if (impl_) impl_->initialX = x; }
void StaticTransform2D::setInitialScaleY(float y) { if (impl_) impl_->initialY = y; }
void StaticTransform2D::setInitialScale(float x, float y) { 
    if (impl_) { impl_->initialX = x; impl_->initialY = y; } 
}

void StaticTransform2D::setAnchorPointX(float x) { if (impl_) impl_->anchorPointX = x; }
void StaticTransform2D::setAnchorPointY(float y) { if (impl_) impl_->anchorPointY = y; }
float StaticTransform2D::anchorPointX() const { return impl_ ? impl_->anchorPointX : 0.0f; }
float StaticTransform2D::anchorPointY() const { return impl_ ? impl_->anchorPointY : 0.0f; }

} // namespace ArtifactCore
