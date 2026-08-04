module;
class tst_QList;
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QtMultimedia/QAudioFormat>
#include <utility>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>

module Audio.SimpleWav;


namespace ArtifactCore {

 class SimpleWav::Impl {
 public:
  QAudioFormat format;
  QVector<float> pcmData; // すべて float (-1.0 ~ 1.0) に正規化して保持する
  qint64 totalFrames = 0;
 };


 SimpleWav::SimpleWav() : impl_(new Impl()) {}

 SimpleWav::~SimpleWav()
 {
  delete impl_;
 }

 bool SimpleWav::loadFromFile(const UniString& filepath, int64_t maxFrames)
 {
  return loadFromFile(filepath.toQString(), maxFrames);
 }

 bool SimpleWav::loadFromFile(const QString& filePath, int64_t maxFrames)
  {
   if (!impl_) {
    return false;
   }

  impl_->pcmData.clear();
  impl_->totalFrames = 0;
  impl_->format = QAudioFormat();

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
   qWarning() << "[SimpleWav] failed to open" << filePath;
   return false;
  }

  QDataStream in(&file);
  in.setByteOrder(QDataStream::LittleEndian);

  char riff[4] = {};
  char wave[4] = {};
  in.readRawData(riff, 4);
  quint32 riffSize = 0;
  in >> riffSize;
  Q_UNUSED(riffSize);
  in.readRawData(wave, 4);

  if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) {
   qWarning() << "[SimpleWav] invalid RIFF/WAVE header for" << filePath;
   return false;
  }
  if (in.status() != QDataStream::Ok) {
   return false;
  }

  quint16 audioFormat = 0;
  quint16 channelCount = 0;
  quint32 sampleRate = 0;
  quint16 bitsPerSample = 0;
  QByteArray dataChunk;

  while (!in.atEnd()) {
   char chunkId[4] = {};
   quint32 chunkSize = 0;
   if (in.readRawData(chunkId, 4) != 4) {
    break;
   }
   in >> chunkSize;

   if (std::memcmp(chunkId, "fmt ", 4) == 0) {
    quint32 byteRate = 0;
    quint16 blockAlign = 0;
    in >> audioFormat;
    in >> channelCount;
    in >> sampleRate;
    in >> byteRate;
    in >> blockAlign;
    in >> bitsPerSample;

    const quint32 remaining = chunkSize > 16 ? chunkSize - 16 : 0;
    if (remaining > 0) {
     file.seek(file.pos() + remaining);
    }
   } else if (std::memcmp(chunkId, "data", 4) == 0) {
    int64_t readBytes = chunkSize;
    if (maxFrames > 0 && channelCount > 0 && bitsPerSample > 0) {
     const int64_t bytesPerSample = bitsPerSample / 8;
     const int64_t channels = channelCount;
     if (bytesPerSample > 0 &&
         maxFrames <= std::numeric_limits<int64_t>::max() /
                           (channels * bytesPerSample)) {
      const int64_t frameBytes = maxFrames * channels * bytesPerSample;
      readBytes = std::min(readBytes, frameBytes);
     }
    }
    if (readBytes <= 0 || readBytes > std::numeric_limits<int>::max()) {
     return false;
    }
    dataChunk.resize(static_cast<int>(readBytes));
    if (in.readRawData(dataChunk.data(), static_cast<int>(readBytes)) != static_cast<int>(readBytes)) {
     qWarning() << "[SimpleWav] failed reading data chunk for" << filePath;
     return false;
    }
   } else {
    file.seek(file.pos() + chunkSize);
   }

   if (chunkSize & 1u) {
    file.seek(file.pos() + 1);
   }
  }

  if (channelCount == 0 || channelCount > 32 || sampleRate < 8000 ||
      sampleRate > 384000 || dataChunk.isEmpty() ||
      bitsPerSample == 0 || bitsPerSample % 8 != 0) {
   qWarning() << "[SimpleWav] incomplete fmt/data chunk for" << filePath
              << "channels=" << channelCount
              << "sampleRate=" << sampleRate
              << "dataBytes=" << dataChunk.size();
   return false;
  }

  impl_->format.setSampleRate(static_cast<int>(sampleRate));
  impl_->format.setChannelCount(static_cast<int>(channelCount));

  const int bytesPerSample = bitsPerSample / 8;
  const int64_t bytesPerFrame = static_cast<int64_t>(channelCount) * bytesPerSample;
  if (bytesPerFrame <= 0 || dataChunk.size() % bytesPerFrame != 0) {
   qWarning() << "[SimpleWav] data chunk is not frame-aligned for" << filePath;
   return false;
  }

  if (audioFormat == 1 && bitsPerSample == 8) {
   impl_->format.setSampleFormat(QAudioFormat::Int16);
   const int sampleCount = dataChunk.size();
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const quint8*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    impl_->pcmData[i] = (static_cast<float>(src[i]) - 128.0f) / 128.0f;
   }
  } else if (audioFormat == 1 && bitsPerSample == 16) {
   impl_->format.setSampleFormat(QAudioFormat::Int16);
   const int sampleCount = dataChunk.size() / static_cast<int>(sizeof(qint16));
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const qint16*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    impl_->pcmData[i] = static_cast<float>(src[i]) / 32768.0f;
   }
  } else if (audioFormat == 1 && bitsPerSample == 24) {
   impl_->format.setSampleFormat(QAudioFormat::Int16);
   const int sampleCount = dataChunk.size() / 3;
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const uchar*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    const int offset = i * 3;
    int32_t value = static_cast<int32_t>(src[offset]) |
                    (static_cast<int32_t>(src[offset + 1]) << 8) |
                    (static_cast<int32_t>(src[offset + 2]) << 16);
    if (value & 0x00800000) value |= ~0x00FFFFFF;
    impl_->pcmData[i] = std::clamp(static_cast<float>(value) / 8388608.0f,
                                   -1.0f, 1.0f);
   }
  } else if (audioFormat == 3 && bitsPerSample == 32) {
   impl_->format.setSampleFormat(QAudioFormat::Float);
   const int sampleCount = dataChunk.size() / static_cast<int>(sizeof(float));
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const float*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    impl_->pcmData[i] = std::isfinite(src[i]) ? std::clamp(src[i], -1.0f, 1.0f) : 0.0f;
   }
  } else {
   qWarning() << "[SimpleWav] unsupported WAV format for" << filePath
              << "audioFormat=" << audioFormat
              << "bitsPerSample=" << bitsPerSample;
   return false;
  }

  impl_->totalFrames = impl_->pcmData.size() / channelCount;
  qDebug() << "[SimpleWav] loaded" << filePath
           << "sampleRate=" << impl_->format.sampleRate()
           << "channels=" << impl_->format.channelCount()
           << "sampleFormat=" << impl_->format.sampleFormat()
           << "frames=" << impl_->totalFrames;
  return impl_->totalFrames > 0;
}

 bool SimpleWav::saveToFile(const UniString& filepath) const
 {
  return saveToFile(filepath.toQString());
 }

 bool SimpleWav::saveToFile(const QString& filePath) const
 {
  if (!impl_ || filePath.isEmpty() || impl_->pcmData.isEmpty() ||
      impl_->format.sampleRate() <= 0 || impl_->format.channelCount() <= 0) {
   return false;
  }
  const qint64 dataBytes = static_cast<qint64>(impl_->pcmData.size()) *
                           static_cast<qint64>(sizeof(float));
  if (dataBytes <= 0 || dataBytes > std::numeric_limits<quint32>::max() - 36) {
   return false;
  }
  if (impl_->totalFrames <= 0 ||
      impl_->totalFrames > std::numeric_limits<quint32>::max()) {
   return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
   return false;
  }
  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  const quint32 payloadSize = static_cast<quint32>(dataBytes);
  out.writeRawData("RIFF", 4);
  out << static_cast<quint32>(36u + payloadSize);
  out.writeRawData("WAVEfmt ", 8);
  out << static_cast<quint32>(16u);
  out << static_cast<quint16>(3u); // IEEE float
  out << static_cast<quint16>(impl_->format.channelCount());
  out << static_cast<quint32>(impl_->format.sampleRate());
  out << static_cast<quint32>(impl_->format.sampleRate() *
                              impl_->format.channelCount() * sizeof(float));
  out << static_cast<quint16>(impl_->format.channelCount() * sizeof(float));
  out << static_cast<quint16>(32u);
  out.writeRawData("data", 4);
  out << payloadSize;
  for (const float value : impl_->pcmData) {
   const float safeValue = std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
   out.writeRawData(reinterpret_cast<const char*>(&safeValue), sizeof(float));
  }
  return out.status() == QDataStream::Ok;
 }

 int SimpleWav::sampleRate() const
 {
  return impl_ ? impl_->format.sampleRate() : 0;
 }

 int SimpleWav::bitDepth() const
 {
  if (!impl_) {
   return 0;
  }
  switch (impl_->format.sampleFormat()) {
  case QAudioFormat::Int16:
   return 16;
  case QAudioFormat::Float:
   return 32;
  default:
   return 0;
  }
 }

 int SimpleWav::channelCount() const
 {
  return impl_ ? impl_->format.channelCount() : 0;
 }

 qint64 SimpleWav::frameCount() const
 {
  return impl_ ? impl_->totalFrames : 0;
 }

 QVector<float> SimpleWav::getAudioData() const
 {
  return impl_ ? impl_->pcmData : QVector<float>();
 }

};
