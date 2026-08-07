module;
#include <QString>
#include <QFile>
#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

module Audio.Render.Writer;

namespace ArtifactCore {

struct AudioWriter::Impl {
    QString filePath;
    QFile file;
    bool isWriting = false;
    quint32 dataBytes = 0;
    quint16 channelCount = 0;
    quint32 sampleRate = 44100;
    quint16 bitDepth = 16;

    Impl() = default;
};

namespace {
void writeWavHeader(QFile& file, quint32 dataBytes,
                    quint16 channels, quint32 sampleRate, quint16 bitDepth)
{
    if (!file.isOpen() || channels == 0 || sampleRate == 0 ||
        (bitDepth != 16 && bitDepth != 24)) return;
    const quint16 bytesPerSample = static_cast<quint16>(bitDepth / 8u);
    const quint32 byteRate = sampleRate * channels * bytesPerSample;
    const quint16 blockAlign = channels * bytesPerSample;
    const quint32 riffSize = 36u + dataBytes;
    file.seek(0);
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << riffSize;
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << channels << sampleRate
           << byteRate << blockAlign << bitDepth;
    stream.writeRawData("data", 4);
    stream << dataBytes;
    file.seek(44 + dataBytes);
}
}

AudioWriter::AudioWriter() : impl_(new Impl()) {}

AudioWriter::~AudioWriter() {
    // Finalize the RIFF/data sizes even when a caller leaves scope without an
    // explicit closeFile().  Without this, the file contains audio bytes but
    // retains the placeholder WAV header and is rejected by many readers.
    closeFile();
    delete impl_;
}

void AudioWriter::openFile(const QString& path) {
    if (!impl_) return;
    closeFile();
    const QString normalized = path.trimmed();
    if (normalized.isEmpty()) return;
    impl_->file.setFileName(normalized);
    if (!impl_->file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        impl_->filePath.clear();
        return;
    }
    impl_->filePath = normalized;
    impl_->dataBytes = 0;
    impl_->channelCount = 0;
    impl_->sampleRate = 44100;
    impl_->isWriting = true;
}

void AudioWriter::setBitDepth(int bitDepth) {
    if (!impl_ || impl_->isWriting || (bitDepth != 16 && bitDepth != 24)) return;
    impl_->bitDepth = static_cast<quint16>(bitDepth);
}

void AudioWriter::closeFile() {
    if (impl_) {
        if (impl_->file.isOpen() && impl_->channelCount > 0) {
            writeWavHeader(impl_->file, impl_->dataBytes,
                           impl_->channelCount, impl_->sampleRate,
                           impl_->bitDepth);
            impl_->file.flush();
        }
        if (impl_->file.isOpen()) impl_->file.close();
        impl_->filePath.clear();
        impl_->isWriting = false;
        impl_->dataBytes = 0;
        impl_->channelCount = 0;
    }
}

void AudioWriter::write(const AudioSegment& segment) {
    if (!impl_ || !impl_->isWriting || !impl_->file.isOpen() ||
        segment.channelData.isEmpty() || segment.sampleRate <= 0) return;
    const int channels = segment.channelCount();
    int frames = segment.frameCount();
    for (const auto& channel : segment.channelData) {
        frames = std::min(frames, static_cast<int>(channel.size()));
    }
    if (channels <= 0 || frames <= 0) return;
    if (impl_->channelCount == 0) {
        impl_->channelCount = static_cast<quint16>(
            std::min(channels, static_cast<int>(std::numeric_limits<quint16>::max())));
        impl_->sampleRate = static_cast<quint32>(segment.sampleRate);
        writeWavHeader(impl_->file, 0, impl_->channelCount, impl_->sampleRate,
                       impl_->bitDepth);
    } else if (static_cast<quint32>(segment.sampleRate) != impl_->sampleRate) {
        // A WAV file has one sample rate in its header.  Appending a segment
        // with a different rate would silently produce timing/pitch corruption.
        return;
    }
    const int writeChannels = std::min(channels, static_cast<int>(impl_->channelCount));
    const quint64 bytesPerSegment = static_cast<quint64>(frames) *
        static_cast<quint64>(writeChannels) * (impl_->bitDepth / 8u);
    constexpr quint64 kMaxRiffDataBytes =
        static_cast<quint64>(std::numeric_limits<quint32>::max()) - 36u;
    if (bytesPerSegment > kMaxRiffDataBytes ||
        static_cast<quint64>(impl_->dataBytes) + bytesPerSegment > kMaxRiffDataBytes) {
        // Classic RIFF cannot represent larger files.  Refuse the segment
        // rather than wrapping the data-size field and emitting a corrupt WAV.
        return;
    }
    QDataStream stream(&impl_->file);
    stream.setByteOrder(QDataStream::LittleEndian);
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < writeChannels; ++channel) {
            const float sample = segment.channelData[channel][frame];
            const float clamped = std::clamp(std::isfinite(sample) ? sample : 0.0f,
                                             -1.0f, 1.0f);
            if (impl_->bitDepth == 24) {
                const qint32 pcm = static_cast<qint32>(std::lrint(clamped * 8388607.0f));
                stream << static_cast<quint8>(pcm & 0xff);
                stream << static_cast<quint8>((pcm >> 8) & 0xff);
                stream << static_cast<quint8>((pcm >> 16) & 0xff);
            } else {
                const qint16 pcm = static_cast<qint16>(std::lrint(clamped * 32767.0f));
                stream << pcm;
            }
        }
    }
    impl_->dataBytes += static_cast<quint32>(bytesPerSegment);
}

} // namespace ArtifactCore
