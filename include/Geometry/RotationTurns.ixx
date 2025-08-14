module;


export module Math.RotationTurns;

import std;

namespace ArtifactCore
{
 class RotationTurns
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  RotationTurns();
  ~RotationTurns();
  RotationTurns(RotationTurns&& other) noexcept;
  RotationTurns& operator=(RotationTurns&& other) noexcept;

  int rotations() const;
  double degrees() const;


 };






};