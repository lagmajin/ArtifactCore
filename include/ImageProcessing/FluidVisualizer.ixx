module;
#include "../Define/DllExportMacro.hpp"
#include <vector>

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
export module ImageProcessing.FluidVisualizer;


import Particle;
import Physics.Fluid;


export namespace ArtifactCore {

class LIBRARY_DLL_API FluidVisualizer {
public:
    struct Style {
        float4 baseColor{0.1f, 0.2f, 0.5f, 1.0f};
        float4 highlightColor{1.0f, 1.0f, 1.0f, 1.0f};
        float densityMultiplier = 1.0f;
        float refractionStrength = 0.05f;
        float edgeThreshold = 0.1f;
        float glowIntensity = 0.5f;
        bool useFireGradient = false;
    };

    /**
     * @brief Apply fluid visual effects to an image buffer
     * @param buffer Target RGBA float buffer
     * @param width Image width
     * @param height Image height
     * @param fluid The fluid solver containing density/velocity data
     * @param style Visual style settings
     */
    void render(float4* buffer, int width, int height, const FluidSolver2D& fluid, const Style& style);

private:
    float4 sampleGradient(float t, const Style& style);
};

} // namespace ArtifactCore
