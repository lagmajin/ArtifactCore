module;
#include <utility>

module Utils.Path;

namespace ArtifactCore {

 class Path::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
 };

 Path::Path() : impl_(new Impl())
 {
 }

 Path::~Path()
 {
  delete impl_;
 }

};
