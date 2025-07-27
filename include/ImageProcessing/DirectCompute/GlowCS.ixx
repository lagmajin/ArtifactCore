module;
#include <QDir>
#include <QString>
#include <opencv2/core/mat.hpp>
export module ImageProcessing.GlowCS;

import Graphics;


export namespace ArtifactCore {

 class GlowCS {
 private:
  class Impl;
  Impl* impl_;
 public:
  GlowCS(DeviceResources& resources);
  ~GlowCS();
  void Process(cv::Mat& mat,bool flip=false);
  void loadShaderBinaryFromDirectory(const QDir& baseDir, const QString& filename);
 };

};