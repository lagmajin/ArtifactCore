#pragma once

#include <QtCore/QObject>


namespace ArtifactCore {

 class AbstractAssetFilePrivate;

 class AbstractAssetFile :public QObject{
 private:

 public:
  AbstractAssetFile();
  virtual ~AbstractAssetFile();
 };



}