#pragma once


//import std.core;

#include <QtCore/QFile>

namespace ArtifactCore {

 class AbstractDecoderPrivate;

 class AbstractDecoder {
 private:

 public:
  AbstractDecoder();
  ~AbstractDecoder();
  virtual void loadFromFile(const QFile& file)=0;
 };





}