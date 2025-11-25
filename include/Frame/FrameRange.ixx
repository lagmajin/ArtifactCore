module;

#include <QtCore/QString>
#include <QtCore/QJsonObject>


export module FrameRange;



namespace ArtifactCore {

 class FrameRange {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameRange();
  ~FrameRange();
 };








};