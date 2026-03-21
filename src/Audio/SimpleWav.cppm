module;
#include <QVector>
#include <QByteArray>
#include <QtMultimedia/QAudioFormat>
module Audio.SimpleWav;


namespace ArtifactCore {

 class SimpleWav::Impl {
 public:
  QAudioFormat format;
  QVector<float> pcmData; // すべて float (-1.0 ~ 1.0) に正規化して保持する
  qint64 totalFrames = 0;

 
 };


 SimpleWav::SimpleWav()
 {

 }

 SimpleWav::~SimpleWav()
 {

 }

 bool SimpleWav::loadFromFile(const UniString& filepath)
 {

  return true;
 }

 int SimpleWav::channelCount() const
 {
  return 0;
 }

 int SimpleWav::sampleRate() const
 {
  return 0;
 }

 int SimpleWav::bitDepth() const
 {
  return 0;
 }

};