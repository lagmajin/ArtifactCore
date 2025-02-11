//#pragma once

//import <memory>;

export module AnchorPoint2D;

export namespace ArtifactCore {


 class AnchorPoint2DPrivate;

 class AnchorPoint2D {
 private:

 public:
  AnchorPoint2D();
  //AnchorPoint2D(const AnchorPoint2D& point);
  
  ~AnchorPoint2D();

  void setX(double x);
  void setY(double y);
 };







};