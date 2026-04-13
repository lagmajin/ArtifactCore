module;

#include <vector>
#include "../Define/DllExportMacro.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>

export module Property.Harmonizer;

import Property.Abstract;
import Time.Rational;

export namespace ArtifactCore {

enum class HarmonizeBaseline {
    Mean,
    Median,
    Selected
};

enum class HarmonizeMode {
    Blend,
    Normalize,
    Distribute
};

struct HarmonizeOptions {
    HarmonizeBaseline baseline = HarmonizeBaseline::Mean;
    HarmonizeMode mode = HarmonizeMode::Blend;
    double intensity = 1.0; 
    int selectedIndex = 0;  // Selected時の基準インデックス
};

class LIBRARY_DLL_API PropertyHarmonizer {
public:
    static void applyHarmonize(std::vector<AbstractProperty*>& properties, const HarmonizeOptions& options);
private:
    static double calculateBaseline(const std::vector<double>& values, const HarmonizeOptions& options);
};

};
