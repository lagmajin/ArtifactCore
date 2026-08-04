module;

#include <opencv2/opencv.hpp>
#include <cmath>
#include <algorithm>

module Track.NccTracker;

namespace ArtifactCore {

static cv::Mat toGray(const ImageF32x4_RGBA& img) {
    cv::Mat m = img.toCVMat();
    cv::Mat gray;
    if (m.channels() >= 3) {
        cv::cvtColor(m, gray, cv::COLOR_RGBA2GRAY);
    } else {
        gray = m.clone();
    }
    return gray;
}

NccResult NccTracker::trackOneFrame(
    const ImageF32x4_RGBA& prevFrame,
    const ImageF32x4_RGBA& currFrame,
    const NccBox& innerBox,
    const NccBox& outerBox,
    bool subpixel)
{
    NccResult result;

    const int pw = prevFrame.width();
    const int ph = prevFrame.height();
    if (pw <= 0 || ph <= 0 || currFrame.width() != pw || currFrame.height() != ph) {
        return result;
    }

    const auto validBox = [](const NccBox& box) {
        return std::isfinite(box.cx) && std::isfinite(box.cy) &&
               std::isfinite(box.halfW) && std::isfinite(box.halfH) &&
               box.halfW > 0.0f && box.halfH > 0.0f;
    };
    if (!validBox(innerBox) || !validBox(outerBox)) {
        return result;
    }

    const int tx = static_cast<int>(innerBox.cx - innerBox.halfW);
    const int ty = static_cast<int>(innerBox.cy - innerBox.halfH);
    const int tw = static_cast<int>(innerBox.halfW * 2.0f);
    const int th = static_cast<int>(innerBox.halfH * 2.0f);

    const int sx = static_cast<int>(outerBox.cx - outerBox.halfW);
    const int sy = static_cast<int>(outerBox.cy - outerBox.halfH);
    const int sw = static_cast<int>(outerBox.halfW * 2.0f);
    const int sh = static_cast<int>(outerBox.halfH * 2.0f);

    if (tw <= 4 || th <= 4 || sw <= tw || sh <= th) {
        result.valid = false;
        return result;
    }

    if (tx < 0 || ty < 0 || tx + tw > pw || ty + th > ph) {
        result.valid = false;
        return result;
    }

    cv::Mat prevGray = toGray(prevFrame);
    cv::Mat currGray = toGray(currFrame);

    cv::Rect tmplRect(tx, ty, tw, th);
    cv::Mat tmpl = prevGray(tmplRect).clone();

    int searchX = std::max(0, sx);
    int searchY = std::max(0, sy);
    int searchW = std::min(sw, currGray.cols - searchX);
    int searchH = std::min(sh, currGray.rows - searchY);

    if (searchW <= tw || searchH <= th) {
        result.valid = false;
        return result;
    }

    cv::Mat searchRegion = currGray(cv::Rect(searchX, searchY, searchW, searchH));

    cv::Mat resultMap;
    // Zero-mean normalized correlation is less sensitive to uniform
    // exposure/black-level changes between consecutive frames.
    cv::matchTemplate(searchRegion, tmpl, resultMap, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(resultMap, &minVal, &maxVal, &minLoc, &maxLoc);

    float peakX = static_cast<float>(searchX + maxLoc.x + tw / 2);
    float peakY = static_cast<float>(searchY + maxLoc.y + th / 2);
    float confidence = static_cast<float>(maxVal);

    if (subpixel && maxLoc.x > 0 && maxLoc.x < resultMap.cols - 1 &&
        maxLoc.y > 0 && maxLoc.y < resultMap.rows - 1) {
        auto val = [&](int dx, int dy) -> float {
            return static_cast<float>(resultMap.at<float>(maxLoc.y + dy, maxLoc.x + dx));
        };
        float c0 = val(0, 0);
        float cx1 = val(-1, 0), cx2 = val(1, 0);
        float cy1 = val(0, -1), cy2 = val(0, 1);

        const float denomX = 2.0f * (2.0f * c0 - cx1 - cx2);
        const float denomY = 2.0f * (2.0f * c0 - cy1 - cy2);
        const float dx = std::abs(denomX) > 1.0e-6f
            ? (cx2 - cx1) / denomX : 0.0f;
        const float dy = std::abs(denomY) > 1.0e-6f
            ? (cy2 - cy1) / denomY : 0.0f;

        if (std::isfinite(dx) && std::isfinite(dy)) {
            peakX += dx;
            peakY += dy;
        }
    }

    result.x = peakX;
    result.y = peakY;
    result.confidence = std::isfinite(confidence)
        ? std::clamp(confidence, 0.0f, 1.0f) : 0.0f;
    result.valid = result.confidence > 0.3f &&
                   std::isfinite(result.x) && std::isfinite(result.y);
    return result;
}

void NccTracker::trackRange(
    std::vector<ImageF32x4_RGBA>& frames,
    const NccBox& innerBox,
    const NccBox& outerBox,
    int startIndex, int endIndex,
    std::vector<NccResult>& results,
    bool subpixel)
{
    results.clear();
    if (frames.empty() || startIndex < 0 || endIndex >= static_cast<int>(frames.size()) ||
        startIndex > endIndex) {
        return;
    }

    results.reserve(static_cast<size_t>(endIndex - startIndex + 1));

    NccBox currentInner = innerBox;
    NccBox currentOuter = outerBox;

    NccResult initial;
    initial.x = currentInner.cx;
    initial.y = currentInner.cy;
    initial.confidence = 1.0f;
    initial.valid = true;
    results.push_back(initial);

    for (int i = startIndex + 1; i <= endIndex; ++i) {
        const ImageF32x4_RGBA& prev = frames[i - 1];
        const ImageF32x4_RGBA& curr = frames[i];

        NccResult r = trackOneFrame(prev, curr, currentInner, currentOuter, subpixel);

        if (r.valid) {
            float dx = r.x - currentInner.cx;
            float dy = r.y - currentInner.cy;
            currentInner.cx = r.x;
            currentInner.cy = r.y;
            currentOuter.cx += dx;
            currentOuter.cy += dy;
        }

        results.push_back(r);
    }
}

}
