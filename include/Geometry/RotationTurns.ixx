module;
#include "../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Math.RotationTurns;





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