module;
#include <QtCore/QFile>

export module MediaProbe;

import MediaInfo;

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



