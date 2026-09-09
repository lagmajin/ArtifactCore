module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>

export module ImageProcessing:FilmRepair;

export namespace ArtifactCore::Repair {

struct FilmRepairParams {
    float threshold = 0.12f;
    float softness = 0.05f;
    float amount = 1.0f;
    float mix = 1.0f;
    int radius = 1;
    bool repairBright = true;
    bool repairDark = true;
    // Temporal single-frame dirt detection (neighbors must agree).
    bool temporalDetect = true;
    float temporalThreshold = 0.08f;
    float motionReject = 0.5f;
    // Fill temporally detected dirt from the neighbor average instead of
    // the spatial median (film dirt exists on one frame only).
    bool temporalRepair = true;
    // Vertical scratch detection (spatial top-hat, persists across frames).
    bool scratchDetect = false;
    float scratchThreshold = 0.1f;
    int scratchMinLength = 24;
    // When true, only the named mask drives repair (no auto detection).
    bool namedMaskOnly = false;
};

struct FilmRepairBuffers {
    const float* src = nullptr;
    float* dst = nullptr;
    // Optional temporal neighbors / pre-built mask (float planes), or null.
    const float* prev = nullptr;
    const float* next = nullptr;
    const float* namedMask = nullptr;
    int width = 0;
    int height = 0;
};

// Full CPU pipeline: spatial median-residual dust mask + temporal dirt mask
// + vertical scratch mask (+ named mask), repaired with a median fill.
// src and dst must be distinct buffers. Returns false for invalid
// dimensions, missing buffers, or an empty repair mask (dst untouched).
bool processFilmRepair(const FilmRepairBuffers& buffers,
                       const FilmRepairParams& params = {});

// Single-frame exposure anomaly (film blink) correction. Unlike continuous
// deflicker smoothing, only frames that deviate from BOTH neighbors are
// touched; neighbor disagreement marks a scene cut and rejects correction.
struct DeBlinkParams {
    double threshold = 0.06;
    double strength = 1.0;
    double sceneCut = 0.25;
    double maxGain = 4.0;
};

// Mean frame luminance of tightly packed RGBA float32 data (subsampled).
// Returns NaN for invalid dimensions or missing data.
double frameLuma(const float* rgba, int width, int height);

// Correction gain for the current frame (1.0 = untouched). Sets applied
// when a blink was detected and corrected.
double deblinkGain(double curLuma, double prevLuma, double nextLuma,
                   const DeBlinkParams& params, bool* applied = nullptr);

} // namespace ArtifactCore::Repair
