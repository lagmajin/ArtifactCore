module;
#include <QString>
export module Core.Position2D;

export namespace ArtifactCore
{

 class Position2D{
 private:
  class Impl;
  Impl* impl_;
 public:
  Position2D();
  ~Position2D();
  Position2D(const Position2D& other);
  Position2D& operator=(const Position2D& other);
  Position2D(Position2D&& other) noexcept;
  Position2D& operator=(Position2D&& other) noexcept;

  void Set(int x, int y);
  void Get(int& x, int& y) const;
 };

 class Position2DF
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Position2DF();
  ~Position2DF();
  Position2DF(const Position2DF& other);
  Position2DF& operator=(const Position2DF& other);
  Position2DF(Position2DF&& other) noexcept;
  Position2DF& operator=(Position2DF&& other) noexcept;

  void Set(float x, float y);
  void Get(float& x, float& y) const;
 };

};