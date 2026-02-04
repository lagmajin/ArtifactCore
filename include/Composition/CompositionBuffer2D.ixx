module ;
#undef emit
//#include <Halide.h>

#include <QtCore/QSize>
#include <QtCore/QObject>
#include <QtCore/QPoint>

#include <QtCore/QScopedPointer>




#include "../Image/FloatImage.hpp"
#include <wobjectdefs.h>
#include "../Define/DllExportMacro.hpp"
export module Composition.Buffer;



import FloatRGBA;

namespace ArtifactCore {


 class LayerSetting {

 };

 enum eEngineBackend {
  OpenCV,
  Halide,

};

 class CompositionBuffer2DPrivate;

 class LIBRARY_DLL_API CompositionBuffer2D:public QObject {
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