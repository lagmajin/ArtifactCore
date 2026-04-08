module;
#include <QString>
#include <QVector>
#include <QByteArray>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QtMultimedia/QAudioFormat>
#include <utility>
#include <cstring>

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

 bool SimpleWav::loadFromFile(const UniString& filepath)
 {
  return loadFromFile(filepath.toQString());
 }

 bool SimpleWav::loadFromFile(const QString& filePath)
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
    dataChunk.resize(static_cast<int>(chunkSize));
    if (in.readRawData(dataChunk.data(), static_cast<int>(chunkSize)) != static_cast<int>(chunkSize)) {
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

  if (channelCount == 0 || sampleRate == 0 || dataChunk.isEmpty()) {
   qWarning() << "[SimpleWav] incomplete fmt/data chunk for" << filePath
              << "channels=" << channelCount
              << "sampleRate=" << sampleRate
              << "dataBytes=" << dataChunk.size();
   return false;
  }

  impl_->format.setSampleRate(static_cast<int>(sampleRate));
  impl_->format.setChannelCount(static_cast<int>(channelCount));

  if (audioFormat == 1 && bitsPerSample == 16) {
   impl_->format.setSampleFormat(QAudioFormat::Int16);
   const int sampleCount = dataChunk.size() / static_cast<int>(sizeof(qint16));
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const qint16*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    impl_->pcmData[i] = static_cast<float>(src[i]) / 32768.0f;
   }
  } else if (audioFormat == 3 && bitsPerSample == 32) {
   impl_->format.setSampleFormat(QAudioFormat::Float);
   const int sampleCount = dataChunk.size() / static_cast<int>(sizeof(float));
   impl_->pcmData.resize(sampleCount);
   const auto* src = reinterpret_cast<const float*>(dataChunk.constData());
   for (int i = 0; i < sampleCount; ++i) {
    impl_->pcmData[i] = src[i];
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
