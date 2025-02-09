#pragma once

#undef emit
#include <Halide.h>

#include <QtCore/QSize>
#include <QtCore/QObject>
#include <QtCore/QPoint>

#include <QtCore/QScopedPointer>




#include "../Image/FloatImage.hpp"
//#include "../Color/FloatRGBA.hpp"

#include <wobjectdefs.h>

import FloatRGBA;

namespace ArtifactCore {


 class LayerSetting {

 };

 enum eEngineBackend {
  OpenCV,
  Halide,

};

 class CompositionBuffer2DPrivate;

 class __declspec(dllexport) CompositionBuffer2D:public QObject {
  //Q_OBJECT
 private:
  QScopedPointer<CompositionBuffer2DPrivate> pImpl_;
 public:
  explicit CompositionBuffer2D(int width,int height);
  ~CompositionBuffer2D();
  void setEngine(eEngineBackend backend=Halide);
 //signals:
  void compositingFinished(); 
  void compositingSucceeded(); 
  void compositingFailed(QString reason); 
 //public slots:
  void addLayer();
  void clear();
  void setClearColor(const FloatRGBA rgba);
 };

};