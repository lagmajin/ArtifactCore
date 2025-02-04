export module ArtifactCore.AbstractDecoder;
#include <QtCore/QFile>




//import std.core;




export namespace ArtifactCore {

 class AbstractDecoderPrivate;

 export class AbstractDecoder {
 private:

 public:
  AbstractDecoder();
  ~AbstractDecoder();
  virtual void loadFromFile(const QFile& file) = 0;
 };

};