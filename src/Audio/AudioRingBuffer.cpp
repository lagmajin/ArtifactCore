module;
#include <QMutexLocker>
module Audio.RingBuffer;

import std;
import Audio.Segment;

namespace ArtifactCore {
 class AudioRingBuffer::Impl {
 private:
  std::vector<float> buffer_;
  size_t capacity_ = 1024; // Default capacity
  size_t readIndex_ = 0;
  size_t writeIndex_ = 0;
  size_t size_ = 0;
  QMutex mutex_;

 public:
  Impl(size_t capacity = 1024) : capacity_(capacity), buffer_(capacity) {}
  ~Impl() {}

  void setCapacity(size_t capacity) {
    QMutexLocker locker(&mutex_);
    capacity_ = capacity;
    buffer_.resize(capacity);
    clear();
  }

  size_t capacity() const {
    return capacity_;
  }

  bool write(const AudioSegment& data) {
    QMutexLocker locker(&mutex_);
    size_t dataSize = data.frameCount();
    if (dataSize > freeSpace()) {
      return false; // Not enough space
    }
    // For simplicity, assume mono or sum channels
    for (size_t i = 0; i < dataSize; ++i) {
      float sample = 0.0f;
      for (int ch = 0; ch < data.channelCount(); ++ch) {
        sample += data.channelData[ch][i];
      }
      sample /= data.channelCount(); // Average
      buffer_[writeIndex_] = sample;
      writeIndex_ = (writeIndex_ + 1) % capacity_;
    }
    size_ += dataSize;
    return true;
  }

  bool read(AudioSegment& data, size_t size) {
    QMutexLocker locker(&mutex_);
    if (size > size_) {
      return false; // Not enough data
    }
    // Assume output is mono
    data.channelData.resize(1);
    data.channelData[0].resize(size);
    for (size_t i = 0; i < size; ++i) {
      data.channelData[0][i] = buffer_[readIndex_];
      readIndex_ = (readIndex_ + 1) % capacity_;
    }
    size_ -= size;
    return true;
  }

  size_t available() const {
    QMutexLocker locker(&mutex_);
    return size_;
  }

  size_t freeSpace() const {
    QMutexLocker locker(&mutex_);
    return capacity_ - size_;
  }

  void clear() {
    QMutexLocker locker(&mutex_);
    readIndex_ = 0;
    writeIndex_ = 0;
    size_ = 0;
  }

  bool isEmpty() const {
    QMutexLocker locker(&mutex_);
    return size_ == 0;
  }

  bool isFull() const {
    QMutexLocker locker(&mutex_);
    return size_ == capacity_;
  }
 };

 AudioRingBuffer::AudioRingBuffer() : impl_(new Impl())
 {

 }

 AudioRingBuffer::AudioRingBuffer(size_t capacity) : impl_(new Impl(capacity))
 {

 }

 AudioRingBuffer::~AudioRingBuffer()
 {
  delete impl_;
 }

 void AudioRingBuffer::setCapacity(size_t capacity)
 {
  impl_->setCapacity(capacity);
 }

 size_t AudioRingBuffer::capacity() const
 {
  return impl_->capacity();
 }

 bool AudioRingBuffer::write(const AudioSegment& data)
 {
  return impl_->write(data);
 }

 bool AudioRingBuffer::read(AudioSegment& data, size_t size)
 {
  return impl_->read(data, size);
 }

 size_t AudioRingBuffer::available() const
 {
  return impl_->available();
 }

 size_t AudioRingBuffer::freeSpace() const
 {
  return impl_->freeSpace();
 }

 void AudioRingBuffer::clear()
 {
  impl_->clear();
 }

 bool AudioRingBuffer::isEmpty() const
 {
  return impl_->isEmpty();
 }

 bool AudioRingBuffer::isFull() const
 {
  return impl_->isFull();
 }

};