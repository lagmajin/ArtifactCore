module;

module Frame.Range;

import std;

namespace ArtifactCore
{
 class FrameRange::Impl
 {
 public:
  Impl();
  ~Impl();
  int start = 0;
  int end = 0;
 };

 FrameRange::Impl::Impl()
 {

 }

 FrameRange::Impl::~Impl()
 {

 }

 FrameRange::FrameRange() : impl_(new Impl())
 {

 }

 FrameRange::FrameRange(int start, int end) : impl_(new Impl())
 {

 }

 FrameRange::FrameRange(const FrameRange& other)
 {

 }

 FrameRange& FrameRange::operator=(const FrameRange& other)
 {

  return *this;
 }

 FrameRange::~FrameRange()
 {

 }

};
