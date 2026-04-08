module;
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
//#include <mutex>
//#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
//#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Script.Python.CoreAPI;

import Script.Python.Engine;

export namespace ArtifactCore {

/**
 * @brief Registers ArtifactCore's built-in utility functions into Python.
 * These are stateless, underlying engine capabilities (Math, Color, DSP, System).
 * 
 * Usage in Python:
 *   import artifact.core
 *   artifact.core.math.distance([0,0], [10,10])
 *   artifact.core.color.hsv_to_rgb(0.5, 1.0, 1.0)
 *   artifact.core.dsp.db_to_linear(-6.0)
 */
class CorePythonAPI {
public:
    static void registerAll();

private:
    static void registerMathAPI();
    static void registerColorAPI();
    static void registerDSPAPI();
    static void registerSystemAPI();
};

} // namespace ArtifactCore
