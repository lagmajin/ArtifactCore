module;
#include <utility>

module Media.TimeStamp;

namespace ArtifactCore {

 class MediaTimeStamp::Impl {
 private:

 public:
  Impl();
  ~Impl();
};

 MediaTimeStamp::Impl::Impl()
 {

 }

 MediaTimeStamp::Impl::~Impl()
 {

 }

 MediaTimeStamp::MediaTimeStamp()
  : impl_(new Impl())
 {
 }

 MediaTimeStamp::~MediaTimeStamp()
 {
  delete impl_;
  impl_ = nullptr;
 }

};
