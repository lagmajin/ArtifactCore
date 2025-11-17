module;
#include <QFile>
#include <QString>

export module Audio.Render.Writer;



export namespace ArtifactCore {

 class AudioWriter {
 private:

 public:
  AudioWriter();
  ~AudioWriter();
  void open();
  void close();
 };



};