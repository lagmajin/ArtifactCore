
//#pragma once


//#include "../Define/DllExportMacro.hpp"


export module Rotate2D;

export namespace ArtifactCore {

 
 class RotatePrivate;

 class  Rotate2D {
 private:
  //QScopedPointer<RotatePrivate> const  pImpl_;
 public:
  Rotate2D();
  Rotate2D(const Rotate2D& other);
  Rotate2D(Rotate2D&& other) noexcept;
  ~Rotate2D();

  //Rotate& operator=(const Rotate& other);
  //Rotate& operator=(Rotate&& other);
 };





}

