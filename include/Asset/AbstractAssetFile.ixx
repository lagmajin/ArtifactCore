module;
#include <QtCore/QObject>

//#include "../File/FileInfo.hpp"

#include <wobjectdefs.h>
export module Asset;
//#pragma once
//import std;






export namespace ArtifactCore {

 class AbstractAssetFilePrivate;

 class AbstractAssetFile :public QObject{
 W_OBJECT(AbstractAssetFile)
 private:

 public:
  AbstractAssetFile();
  virtual ~AbstractAssetFile();
 //public slots:
  
 };



}