module;
#include <QtCore/QFile>

export module Media.MediaProbe;

import Media.Info;

export namespace ArtifactCore {

 class MediaProbePrivate;

 class MediaProbe {
 private:

 public:
  MediaProbe();
  ~MediaProbe();
  void open(const QFile& file);
 };



};



