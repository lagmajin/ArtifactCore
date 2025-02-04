#include <QtCore/QFile>


//export moArtifactCore.AbstractDecoder;
//import std.core;




export namespace ArtifactCore {

 class AbstractDecoderPrivate;

 class AbstractDecoder {
 private:

 public:
  AbstractDecoder();
  ~AbstractDecoder();
  virtual void loadFromFile(const QFile& file)=0;
 };





}