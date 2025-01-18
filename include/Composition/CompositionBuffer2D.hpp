#pragma once


#include <QtCore/QSize>
#include <QtCore/QObject>
#include <QtCore/QPoint>


#include "../Image/FloatImage.hpp"



namespace ArtifactCore {


 class LayerSetting {
  QPoint position;
  float opacity = 1.0f;
 };


 class CompositionBufferPrivate;

 class CompositionBuffer2D:public QObject {
  Q_OBJECT
 private:

 public:
  CompositionBuffer2D();
  ~CompositionBuffer2D();

 signals:
  void compositingFinished(); 
  void compositingSucceeded(); 
  void compositingFailed(QString reason); 
 public slots:
  void addLayer();

 };

};