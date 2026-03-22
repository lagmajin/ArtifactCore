module;
#include <cmath>
#include <utility>
export module ShapeVertex;



export namespace ArtifactCore {

 class ShapeVertex {
 public:
  struct Vec2 {
   float x = 0.0f;
   float y = 0.0f;

   Vec2() = default;
   Vec2(float x, float y) : x(x), y(y) {}

   Vec2 operator+(const Vec2& rhs) const { return { x + rhs.x, y + rhs.y }; }
   Vec2 operator-(const Vec2& rhs) const { return { x - rhs.x, y - rhs.y }; }
   Vec2 operator*(float s) const { return { x * s, y * s }; }
   float length() const { return std::sqrt(x * x + y * y); }
   void normalize() {
	float len = length();
	if (len > 1e-6f) { x /= len; y /= len; }
   }
  };

 private:
  Vec2 pos_;       // ���_�ʒu
  Vec2 inHandle_;  // ����n���h���i���΁j
  Vec2 outHandle_; // �o�n���h���i���΁j
  bool corner_ = false; // �X���[�Y or �R�[�i�[

 public:
  ShapeVertex() = default;
  explicit ShapeVertex(Vec2 pos) : pos_(pos) {}

  const Vec2& position() const { return pos_; }
  void setPosition(const Vec2& p) { pos_ = p; }

  const Vec2& inHandle() const { return inHandle_; }
  void setInHandle(const Vec2& h) { inHandle_ = h; }

  const Vec2& outHandle() const { return outHandle_; }
  void setOutHandle(const Vec2& h) { outHandle_ = h; }

  bool isCorner() const { return corner_; }
  void setCorner(bool corner) { corner_ = corner; }

  // world���W�ł̃n���h���ʒu
  Vec2 inHandleWorld() const { return pos_ + inHandle_; }
  Vec2 outHandleWorld() const { return pos_ + outHandle_; }

  void mirrorHandles(bool keepLength = true) {
   if (keepLength) {
	outHandle_ = inHandle_ * -1.0f;
   }
   else {
	outHandle_ = { -inHandle_.x, -inHandle_.y };
   }
  }
 };












}