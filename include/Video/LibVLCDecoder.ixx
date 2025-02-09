module; 
#include <stdint.h>
#include <memory>

#include <QtCore/QFile>

export module Codec;

export namespace ArtifactCore {

  class LibVLCDecoderPrivate;

  class LibVLCDecoder {
  private:

  public:
   LibVLCDecoder();
   ~LibVLCDecoder();

   void loadFromFile(const QFile& file);
   void seekiAtTime();
   void frameExtractAtSeek();
  };

};