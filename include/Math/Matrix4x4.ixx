module;
#include <array>
#include <algorithm>
#include <cmath>

export module Math.Matrix4x4;

export namespace ArtifactCore {

struct Matrix4x4 {
  std::array<float, 16> m{};

  static Matrix4x4 identity() {
    Matrix4x4 out;
    out.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    return out;
  }

  static Matrix4x4 translation(float x, float y, float z = 0.0f) {
    Matrix4x4 out = identity();
    out.m[3] = x;
    out.m[7] = y;
    out.m[11] = z;
    return out;
  }

  static Matrix4x4 scale(float x, float y, float z = 1.0f) {
    Matrix4x4 out = identity();
    out.m[0] = x;
    out.m[5] = y;
    out.m[10] = z;
    return out;
  }

  static Matrix4x4 rotationZ(float degrees) {
    const float radians = degrees * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4x4 out = identity();
    out.m[0] = c;
    out.m[1] = -s;
    out.m[4] = s;
    out.m[5] = c;
    return out;
  }

  static Matrix4x4 multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 out;
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        float v = 0.0f;
        for (int k = 0; k < 4; ++k) {
          v += a.m[row * 4 + k] * b.m[k * 4 + col];
        }
        out.m[row * 4 + col] = v;
      }
    }
    return out;
  }
};

} // namespace ArtifactCore
