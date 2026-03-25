module;
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <vector>
#include <algorithm>

module Audio.RingBuffer;

import Audio.Segment;

namespace ArtifactCore {
    class AudioRingBuffer::Impl {
    private:
        std::vector<std::vector<float>> channels_;
        size_t capacity_ = 48000; 
        size_t readIndex_ = 0;
        size_t writeIndex_ = 0;
        size_t size_ = 0;
        int channelCount_ = 2;
        mutable QMutex mutex_;

    public:
        Impl(size_t capacity = 48000) : capacity_(capacity) {
            channels_.resize(channelCount_);
            for (auto& ch : channels_) ch.resize(capacity_);
        }

        void setCapacity(size_t capacity) {
            QMutexLocker locker(&mutex_);
            capacity_ = capacity;
            for (auto& ch : channels_) ch.resize(capacity_);
            clearInternal();
        }

        size_t capacity() const {
            return capacity_;
        }

        bool write(const AudioSegment& data) {
            QMutexLocker locker(&mutex_);
            size_t frames = data.frameCount();
            if (frames > freeSpaceInternal()) {
                return false; 
            }

            // Ensure our internal storage matches the channel count of the incoming data, or at least common ground (Stereo)
            // For now, we stick to Stereo internal buffer if initialized so.
            
            for (int i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channelCount_; ++ch) {
                    float sample = 0.0f;
                    if (data.channelCount() > ch) {
                        sample = data.channelData[ch][i];
                    } else if (data.channelCount() == 1) {
                        // Upmix mono to all channels
                        sample = data.channelData[0][i];
                    }
                    channels_[ch][writeIndex_] = sample;
                }
                writeIndex_ = (writeIndex_ + 1) % capacity_;
            }
            size_ += frames;
            return true;
        }

        bool read(AudioSegment& data, size_t frames) {
            QMutexLocker locker(&mutex_);
            if (size_ == 0) {
                data.clear();
                return false;
            }

            const size_t readableFrames = std::min(frames, size_);

            data.channelData.resize(channelCount_);
            for (int ch = 0; ch < channelCount_; ++ch) {
                data.channelData[ch].resize(readableFrames);
                size_t tempReadIdx = readIndex_;
                for (size_t i = 0; i < readableFrames; ++i) {
                    data.channelData[ch][i] = channels_[ch][tempReadIdx];
                    tempReadIdx = (tempReadIdx + 1) % capacity_;
                }
            }
            
            readIndex_ = (readIndex_ + readableFrames) % capacity_;
            size_ -= readableFrames;
            return readableFrames > 0;
        }

        size_t available() const {
            QMutexLocker locker(&mutex_);
            return size_;
        }

        size_t freeSpaceInternal() const {
            return capacity_ - size_;
        }

        size_t freeSpace() const {
            QMutexLocker locker(&mutex_);
            return freeSpaceInternal();
        }

        void clearInternal() {
            readIndex_ = 0;
            writeIndex_ = 0;
            size_ = 0;
        }

        void clear() {
            QMutexLocker locker(&mutex_);
            clearInternal();
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

    AudioRingBuffer::AudioRingBuffer() : impl_(new Impl()) {}
    AudioRingBuffer::AudioRingBuffer(size_t capacity) : impl_(new Impl(capacity)) {}
    AudioRingBuffer::~AudioRingBuffer() { delete impl_; }

    void AudioRingBuffer::setCapacity(size_t capacity) { impl_->setCapacity(capacity); }
    size_t AudioRingBuffer::capacity() const { return impl_->capacity(); }
    bool AudioRingBuffer::write(const AudioSegment& data) { return impl_->write(data); }
    bool AudioRingBuffer::read(AudioSegment& data, size_t size) { return impl_->read(data, size); }
    size_t AudioRingBuffer::available() const { return impl_->available(); }
    size_t AudioRingBuffer::freeSpace() const { return impl_->freeSpace(); }
    void AudioRingBuffer::clear() { impl_->clear(); }
    bool AudioRingBuffer::isEmpty() const { return impl_->isEmpty(); }
    bool AudioRingBuffer::isFull() const { return impl_->isFull(); }
}
