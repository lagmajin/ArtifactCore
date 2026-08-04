module;

#include <utility>

module File:Info;

namespace ArtifactCore {

class FileInfo::Impl {
public:
    Impl() = default;
    ~Impl() = default;
};

FileInfo::FileInfo() : impl_(new Impl()) {}

FileInfo::~FileInfo()
{
    delete impl_;
    impl_ = nullptr;
}

}
