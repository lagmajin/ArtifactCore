#pragma once

#include <QtGui/QTransform>

#include "../third_party/Eigen/Core"

namespace ArtifactCore {

	class Scale2DPrivate;

	class Scale2D {

		Scale2D();
		explicit Scale2D(double x, double y);
		~Scale2D();
		double x() const;
		double y() const;
		void setScale(double x, double y);

	};






};