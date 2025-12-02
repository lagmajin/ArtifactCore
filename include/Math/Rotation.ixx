module;
export module Math.Rotation;

import std;

export namespace ArtifactCore
{
 class Rotation
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Rotation();
  Rotation(const Rotation& other);
  Rotation(Rotation&& other) noexcept;
  ~Rotation();
 	
  Rotation& operator=(const Rotation& other);
 	
 };
	
	
	
};