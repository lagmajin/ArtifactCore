#pragma once

#include <QtGui/QpaintDevice>
#include "CvMatPaintEngine.hpp"

namespace ArtifactCore {

 class MyPaintDevice : public QPaintDevice {
 public:
  MyPaintDevice() : engine() {}
  ~MyPaintDevice() { delete engine; }

 protected:
  int devType() const override {
   return 0;
  }

  QPaintEngine* paintEngine() const override {
   return engine;
  }

 private:
  CvMatPaintEngine* engine;
 };





}