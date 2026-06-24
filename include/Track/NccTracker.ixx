module;
#include <cstdint>
#include <vector>
#include <cmath>

export module Track.NccTracker;

import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

struct NccResult {
    float x = 0.0f;
    float y = 0.0f;
    float confidence = 0.0f;
    bool valid = false;
};

struct NccBox {
    float cx = 0.0f;
    float cy = 0.0f;
    float halfW = 16.0f;
    float halfH = 16.0f;
};

class NccTracker {
public:
    static NccResult trackOneFrame(
        const ImageF32x4_RGBA& prevFrame,
        const ImageF32x4_RGBA& currFrame,
        const NccBox& innerBox,
        const NccBox& outerBox,
        bool subpixel = true);

    static void trackRange(
        std::vector<ImageF32x4_RGBA>& frames,
        const NccBox& innerBox,
        const NccBox& outerBox,
        int startIndex, int endIndex,
        std::vector<NccResult>& results,
        bool subpixel = true);
};

}

