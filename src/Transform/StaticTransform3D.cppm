module;
#include <utility>

module Transform._3D;

namespace ArtifactCore {

struct StaticTransform3D::Impl {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    float rotationX = 0.0f, rotationY = 0.0f, rotationZ = 0.0f;

    Impl() = default;
};

StaticTransform3D::StaticTransform3D() : impl_(new Impl()) {}

StaticTransform3D::StaticTransform3D(const StaticTransform3D& other) 
    : impl_(new Impl(*other.impl_)) {}

StaticTransform3D::StaticTransform3D(StaticTransform3D&& other) noexcept 
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

StaticTransform3D::~StaticTransform3D() {
    delete impl_;
}

StaticTransform3D& StaticTransform3D::operator=(const StaticTransform3D& other) {
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

StaticTransform3D& StaticTransform3D::operator=(StaticTransform3D&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

} // namespace ArtifactCore
