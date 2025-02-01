#pragma once


#include <memory>



#include "AbstractDecoder.hpp"


namespace ArtifactCore {

 class LibVLCDecoderPrivate;

 class LibVLCDecoder:public AbstractDecoder {
 private:

 public:
  LibVLCDecoder();
  ~LibVLCDecoder();

  void loadFromFile(const QFile& file) override;
  void seekiAtTime();
  void frameExtractAtSeek();
 };







};