module;
#include <cmath>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Math.RotationTurns;





namespace ArtifactCore
{
	class RotationTurns::Impl
	{
	public:
		int rotations = 0;
		double degrees = 0.0;

		void normalize()
		{
			double whole = std::floor(degrees / 360.0);
			rotations += static_cast<int>(whole);
			degrees -= whole * 360.0;
			// Ensure positive range [0, 360)
			if (degrees < 0.0) {
				degrees += 360.0;
				rotations -= 1;
			}
			// Precision fix if needed? 360.0 should be wrapped to 0?
			// if (degrees >= 360.0) { degrees = 0.0; rotations++; } // floor handles this.
		}
	};

	RotationTurns::RotationTurns()
		: impl_(new Impl())
	{
	}

	RotationTurns::RotationTurns(int rotations, double degrees)
		: impl_(new Impl())
	{
		impl_->rotations = rotations;
		impl_->degrees = degrees;
		impl_->normalize();
	}

	RotationTurns::RotationTurns(double totalDegrees)
		: impl_(new Impl())
	{
		impl_->degrees = totalDegrees;
		impl_->normalize();
	}

	RotationTurns::~RotationTurns()
	{
		delete impl_;
	}

	RotationTurns::RotationTurns(RotationTurns&& other) noexcept
		: impl_(other.impl_)
	{
		other.impl_ = nullptr;
	}

	RotationTurns& RotationTurns::operator=(RotationTurns&& other) noexcept
	{
		if (this != &other)
		{
			delete impl_;
			impl_ = other.impl_;
			other.impl_ = nullptr;
		}
		return *this;
	}

	int RotationTurns::rotations() const
	{
		return impl_->rotations;
	}

	double RotationTurns::degrees() const
	{
		return impl_->degrees;
	}

	void RotationTurns::set(int rotations, double degrees)
	{
		impl_->rotations = rotations;
		impl_->degrees = degrees;
		impl_->normalize();
	}

	void RotationTurns::setTotalDegrees(double totalDegrees)
	{
		impl_->rotations = 0;
		impl_->degrees = totalDegrees;
		impl_->normalize();
	}

	double RotationTurns::getTotalDegrees() const
	{
		return static_cast<double>(impl_->rotations) * 360.0 + impl_->degrees;
	}

	void RotationTurns::setRotations(int rotations)
	{
		impl_->rotations = rotations;
	}

	void RotationTurns::setDegrees(double degrees)
	{
		impl_->degrees = degrees;
		impl_->normalize();
	}
}
