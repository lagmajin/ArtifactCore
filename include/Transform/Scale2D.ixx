module;

#include <memory>
#include <stdint.h>
#include <QtGui/QTransform>

#include "../Define/DllExportMacro.hpp"

//import std;
export module Scale2D;
//#include "../third_party/Eigen/Core"
//import std;

export namespace ArtifactCore {

	class Scale2DPrivate;

	class LIBRARY_DLL_API Scale2D {
	private:
	//Scale2DPrivate
	public:
		Scale2D();
		explicit Scale2D(double x, double y);
		~Scale2D();
		double x() const;
		double y() const;
		void setScale(double x, double y);
		QTransform ToQTransform() const;
	};






};