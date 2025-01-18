#pragma once


#include <Halide.h>


namespace ArtifactCore {

 using namespace Halide;

 Func add_images(Func img1, Func img2, std::string name = "added") 
 {
  Var x("x"), y("y"), channel("c");

  Func add(name);
  add(x, y, channel) = img1(x, y, channel) + img2(x, y, channel);

  return add;

 }



};