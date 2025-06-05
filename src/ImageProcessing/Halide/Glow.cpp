
#include <Halide.h>
using namespace Halide;



namespace ArtifactCore {
 using namespace Halide;

 Expr screen(Expr a, Expr b) {
  return 1.0f - (1.0f - a) * (1.0f - b);
 }
 Func apply_multiblur_screen(Buffer<float> input) {
  Var x, y, c;

  Func clamped = BoundaryConditions::repeat_edge(input);

  Func input_f("input_f");
  input_f(x, y, c) = clamped(x, y, c);

  // ブラー段階
  const std::vector<int> sigmas = { 2, 4, 8, 16 };
  std::vector<Func> blurs;

  for (int s : sigmas) {
   Func blurx("blurx_" + std::to_string(s)), blury("blury_" + std::to_string(s));

   blurx(x, y, c) = (input_f(x - 1, y, c) + 2.0f * input_f(x, y, c) + input_f(x + 1, y, c)) / 4.0f;
   blury(x, y, c) = (blurx(x, y - 1, c) + 2.0f * blurx(x, y, c) + blurx(x, y + 1, c)) / 4.0f;

   // NOTE: 実際は GaussianApprox などを使った方がいい
   blurs.push_back(blury);
  }

  // スクリーン合成
  Func result("result");
  Expr accum = input_f(x, y, c);

  for (auto& b : blurs) {
   accum = 1.0f - (1.0f - accum) * (1.0f - b(x, y, c));
  }

  result(x, y, c) = accum;
  return result;
 }






}