#pragma once
#include <stdint.h>
#include <QtCore/QObject>


namespace ArtifactCore {

 class ArtifactMetaFilePrivate;

 class ArtifactMenuFile {
 private:
  ArtifactMetaFilePrivate* pFile_;
 public:
  ArtifactMenuFile();
  ~ArtifactMenuFile();
 };








};