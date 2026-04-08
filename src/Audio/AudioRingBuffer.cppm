module;
module Audio.RingBuffer;

import std;
import Audio.Segment;

namespace ArtifactCore {
    // Single-producer / single-consumer (SPSC) lock-free ring buffer.
    // Producer = PlaybackEngine thread  (write, clear, available)
    // Consumer = WASAPI render thread   (read)
    class AudioRingBuffer::Impl {
        std::vector<std::vector<float>> channels_;
        std::size_t capacity_ = 48000 * 8;
        int channelCount_ = 2;

        // writeCount_ is exclusively written by the producer.
        // readCount_ is exclusively written by the consumer.
        // Placing them on separate cache lines prevents false sharing.
        alignas(64) std::atomic<std::uint64_t> writeCount_{0};
        alignas(64) std::atomic<std::uint64_t> readCount_{0};

        // The producer increments clearGeneration_ to tell the consumer that
        // all previously buffered frames should be discarded.
        std::atomic<std::uint32_t> clearGeneration_{0};
        std::uint32_t lastClearGen_ = 0; // consumer-side; no atomic needed

    public:
        explicit Impl(std::size_t capacity = 48000 * 8) : capacity_(capacity) {
            channels_.resize(channelCount_);
            for (auto& ch : channels_) ch.resize(capacity_);
        }

        // Must only be called while audio output is stopped.
        void setCapacity(std::size_t capacity) {
            capacity_ = capacity;
            for (auto& ch : channels_) ch.resize(capacity_);
            writeCount_.store(0, std::memory_order_relaxed);
            readCount_.store(0, std::memory_order_relaxed);
            clearGeneration_.fetch_add(1, std::memory_order_release);
        }

        std::size_t capacity() const { return capacity_; }

        std::size_t available() const {
            const std::uint64_t w = writeCount_.load(std::memory_order_acquire);
            const std::uint64_t r = readCount_.load(std::memory_order_acquire);
            return static_cast<std::size_t>(w - r);
        }

        std::size_t freeSpace() const {
            return capacity_ - available();
        }

        bool write(const AudioSegment& data) {
            const std::size_t frames = data.frameCount();
            if (frames == 0) return true;
            const std::uint64_t r = readCount_.load(std::memory_order_acquire);
            const std::uint64_t w = writeCount_.load(std::memory_order_relaxed);
            if (static_cast<std::size_t>(w - r) + frames > capacity_) {
                return false;
            }

            for (int ch = 0; ch < channelCount_; ++ch) {
                const float* src = nullptr;
                if (data.channelCount() > ch) {
                    src = data.channelData[ch].data();
                } else if (data.channelCount() == 1) {
                    src = data.channelData[0].data();
                }

                const std::size_t wIdx = static_cast<std::size_t>(w) % capacity_;
                const std::size_t firstChunk = std::min(frames, capacity_ - wIdx);

                if (src) {
                    std::memcpy(&channels_[ch][wIdx], src, firstChunk * sizeof(float));
                    if (firstChunk < frames) {
                        std::memcpy(&channels_[ch][0], src + firstChunk, (frames - firstChunk) * sizeof(float));
                    }
                } else {
                    std::memset(&channels_[ch][wIdx], 0, firstChunk * sizeof(float));
                    if (firstChunk < frames) {
                        std::memset(&channels_[ch][0], 0, (frames - firstChunk) * sizeof(float));
                    }
                }
            }

            writeCount_.store(w + frames, std::memory_order_release);
            return true;
        }

        bool read(AudioSegment& data, std::size_t frames) {
            // Check whether the producer has requested a buffer clear.
            const std::uint32_t gen = clearGeneration_.load(std::memory_order_acquire);
            if (gen != lastClearGen_) {
                // Advance readCount to writeCount so the buffer appears empty.
                readCount_.store(writeCount_.load(std::memory_order_acquire),
                                 std::memory_order_release);
                lastClearGen_ = gen;
                data.clear();
                return false;
            }

            const std::uint64_t r = readCount_.load(std::memory_order_relaxed);
            const std::uint64_t w = writeCount_.load(std::memory_order_acquire);
            const std::size_t avail = static_cast<std::size_t>(w - r);
            if (avail == 0) {
                data.clear();
                return false;
            }

            const std::size_t readFrames = std::min(frames, avail);
            data.channelData.resize(channelCount_);
            for (int ch = 0; ch < channelCount_; ++ch) {
                data.channelData[ch].resize(readFrames);
                const std::size_t rIdx = static_cast<std::size_t>(r) % capacity_;
                const std::size_t firstChunk = std::min(readFrames, capacity_ - rIdx);

                std::memcpy(data.channelData[ch].data(), &channels_[ch][rIdx], firstChunk * sizeof(float));
                if (firstChunk < readFrames) {
                    std::memcpy(data.channelData[ch].data() + firstChunk, &channels_[ch][0], (readFrames - firstChunk) * sizeof(float));
                }
            }
            readCount_.store(r + readFrames, std::memory_order_release);
            return true;
        }

        // Called by the producer to discard all buffered audio.
        // The consumer will detect the generation change on its next read().
        void clear() {
            clearGeneration_.fetch_add(1, std::memory_order_release);
        }

        bool isEmpty() const { return available() == 0; }
        bool isFull() const { return available() >= capacity_; }
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
