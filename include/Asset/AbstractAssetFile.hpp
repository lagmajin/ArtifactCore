#pragma once

#include <QtCore/QObject>

#include "../File/FileInfo.hpp"

namespace ArtifactCore {

 class AbstractAssetFilePrivate;

 class AbstractAssetFile :public QObject{
 private:

 public:
  AbstractAssetFile();
  virtual ~AbstractAssetFile();
 public slots:
  
 };



}