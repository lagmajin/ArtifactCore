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
		RotationTurns(int rotations, double degrees);
		explicit RotationTurns(double totalDegrees);

		~RotationTurns();
		RotationTurns(RotationTurns&& other) noexcept;
		RotationTurns& operator=(RotationTurns&& other) noexcept;

		int rotations() const;
		double degrees() const;

		void set(int rotations, double degrees);
		void setTotalDegrees(double totalDegrees);
		double getTotalDegrees() const;

		void setRotations(int rotations);
		void setDegrees(double degrees);


 };






};