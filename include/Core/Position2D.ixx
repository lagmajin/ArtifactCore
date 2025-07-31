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
 };

 class Position2DF
 {private:
  class Impl;
  Impl* impl_;
 public:
	Position2DF();
	~Position2DF();
	void Set(float x, float y);
	void Get(float& x, float& y) const;
 };



};