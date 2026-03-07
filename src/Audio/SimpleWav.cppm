module;
#include <QVector>
#include <QByteArray>
#include <QtMultimedia/QAudioFormat>
module Audio.SimpleWav;


namespace ArtifactCore {

 class SimpleWav::Impl {
 public:
  QAudioFormat format;
  QVector<float> pcmData; // ‚·‚×‚Ä float (-1.0 ~ 1.0) ‚É³‹K‰»‚µ‚Ä‚Â‚Ì‚ªQImage“I
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