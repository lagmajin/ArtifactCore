#pragma once



#include <Halide.h>


namespace ArtifactCore {

 using namespace Halide;

 Func MergeAdd(Func imageA, Func imageB,
  Expr x_offset_A, Expr y_offset_A,
  Expr x_offset_B, Expr y_offset_B) {
  Var x, y;

  // 画像Aと画像Bの位置をアンカーポイントを基準に調整して加算
  Func result;
  result(x, y) = select(
   (x - x_offset_A >= 0) && (x - x_offset_B >= 0) && (y - y_offset_A >= 0) && (y - y_offset_B >= 0),
   imageA(x - x_offset_A, y - y_offset_A) + imageB(x - x_offset_B, y - y_offset_B),
   0);  // 範囲外のピクセルはゼロにする
  return result;
 }







}