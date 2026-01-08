module;
#include "../Define/DllExportMacro.hpp"

export module Math.RotationTurns;

import std;

export namespace ArtifactCore
{
 class LIBRARY_DLL_API RotationTurns
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