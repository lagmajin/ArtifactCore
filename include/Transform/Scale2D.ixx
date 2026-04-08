module;


#include <QtGui/QTransform>

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
export module Transform.Scale2D;




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