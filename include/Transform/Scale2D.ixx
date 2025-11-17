module;


#include <QtGui/QTransform>

#include "../Define/DllExportMacro.hpp"



export module Transform.Scale2D;
import std;

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