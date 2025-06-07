#pragma once
//import std;

#include <QtCore/QObject>

#include "../File/FileInfo.hpp"

#include <wobjectdefs.h>




namespace ArtifactCore {

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