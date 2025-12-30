module;
#include <QMutexLocker>
module Audio.RingBuffer;

import std;
import Audio.Segment;

namespace ArtifactCore {
 class AudioRingBuffer::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 AudioRingBuffer::Impl::Impl()
 {

 }

 AudioRingBuffer::AudioRingBuffer() : impl_(new Impl())
 {

 }
 AudioRingBuffer::~AudioRingBuffer()
 {
  delete impl_;
 }
};