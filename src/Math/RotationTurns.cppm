module;
#include <cmath>

module Math.RotationTurns;

import std;

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
