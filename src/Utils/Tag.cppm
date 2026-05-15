module;
#include <utility>
#define QT_NO_KEYWORDS
#include <QString>

module Utils.Tag;

import Utils.String.UniString;

namespace ArtifactCore {

class Tag::Impl {
public:
    UniString name;
    Impl() = default;
    Impl(const UniString& n) : name(n) {}
};

Tag::Tag() : impl_(new Impl()) {}

Tag::Tag(const UniString& name) : impl_(new Impl(name)) {}

Tag::Tag(const Tag& other) : impl_(new Impl(other.impl_->name)) {}

Tag::Tag(Tag&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

Tag::~Tag() {
    delete impl_;
}

void Tag::setName(const UniString& name) {
    if (impl_) impl_->name = name;
}

UniString Tag::name() const {
    return impl_ ? impl_->name : UniString();
}

Tag& Tag::operator=(const Tag& other) {
    if (this != &other) {
        if (impl_) impl_->name = other.impl_->name;
        else impl_ = new Impl(other.impl_->name);
    }
    return *this;
}

Tag& Tag::operator=(Tag&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

} // namespace ArtifactCore
