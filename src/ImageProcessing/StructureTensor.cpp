module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>

module ImageProcessing;

import :StructureTensor;

namespace ArtifactCore {

TensorField StructureTensor::analyze(const ImageF32x4_RGBA& image, float rGaussianWidth, float tGaussianWidth) {
    if (image.isEmpty()) return TensorField{};
    cv::Mat mat = image.toCVMat();
    return analyzeMat(&mat, rGaussianWidth, tGaussianWidth);
}

TensorField StructureTensor::analyzeMat(const void* cvMatPtr, float rGaussianWidth, float tGaussianWidth) {
    if (!cvMatPtr) return TensorField{};
    const cv::Mat& srcMat = *static_cast<const cv::Mat*>(cvMatPtr);
    if (srcMat.empty()) return TensorField{};

    int w = srcMat.cols;
    int h = srcMat.rows;

    // Convert to grayscale CV_32FC1
    cv::Mat gray;
    if (srcMat.channels() == 4) {
        cv::cvtColor(srcMat, gray, cv::COLOR_BGRA2GRAY);
    } else if (srcMat.channels() == 3) {
        cv::cvtColor(srcMat, gray, cv::COLOR_BGR2GRAY);
    } else {
        srcMat.convertTo(gray, CV_32F);
    }

    // 1. Noise suppression
    if (rGaussianWidth > 0.0f) {
        int ksize = static_cast<int>(std::round(rGaussianWidth * 3.0f)) * 2 + 1;
        cv::GaussianBlur(gray, gray, cv::Size(ksize, ksize), rGaussianWidth);
    }

    // 2. Compute spatial derivatives
    cv::Mat Ix, Iy;
    cv::Sobel(gray, Ix, CV_32F, 1, 0, 3);
    cv::Sobel(gray, Iy, CV_32F, 0, 1, 3);

    // 3. Compute tensor components
    cv::Mat Jxx = Ix.mul(Ix);
    cv::Mat Jyy = Iy.mul(Iy);
    cv::Mat Jxy = Ix.mul(Iy);

    // 4. Integrate tensor components locally
    if (tGaussianWidth > 0.0f) {
        int ksize = static_cast<int>(std::round(tGaussianWidth * 3.0f)) * 2 + 1;
        cv::GaussianBlur(Jxx, Jxx, cv::Size(ksize, ksize), tGaussianWidth);
        cv::GaussianBlur(Jyy, Jyy, cv::Size(ksize, ksize), tGaussianWidth);
        cv::GaussianBlur(Jxy, Jxy, cv::Size(ksize, ksize), tGaussianWidth);
    }

    // 5. Initialize Output Buffers
    TensorField field;
    field.width = w;
    field.height = h;
    field.angles.resize(static_cast<size_t>(w) * h, 0.0f);
    field.coherence.resize(static_cast<size_t>(w) * h, 0.0f);
    field.magnitude.resize(static_cast<size_t>(w) * h, 0.0f);

    // 6. Eigenvalue analysis per pixel
    for (int y = 0; y < h; ++y) {
        const float* pJxx = Jxx.ptr<float>(y);
        const float* pJyy = Jyy.ptr<float>(y);
        const float* pJxy = Jxy.ptr<float>(y);

        for (int x = 0; x < w; ++x) {
            float jxx = pJxx[x];
            float jyy = pJyy[x];
            float jxy = pJxy[x];

            // Tensor trace and determinant
            float trace = jxx + jyy;
            // sqrt term for eigenvalues: sqrt((jxx - jyy)^2 + 4 * jxy^2)
            float diff = jxx - jyy;
            float term = std::sqrt(diff * diff + 4.0f * jxy * jxy);

            // Eigenvalues
            float l1 = 0.5f * (trace + term); // primary eigenvalue (gradient magnitude direction)
            float l2 = 0.5f * (trace - term); // secondary eigenvalue (edge/flow direction)

            // Flow orientation is perpendicular to gradient direction
            // Angle theta of flow (minimum gradient direction)
            float theta = 0.5f * std::atan2(2.0f * jxy, diff) + 3.14159265f * 0.5f;

            // Normalize angle to [-PI/2, PI/2]
            if (theta > 3.14159265f * 0.5f) {
                theta -= 3.14159265f;
            } else if (theta < -3.14159265f * 0.5f) {
                theta += 3.14159265f;
            }

            // Coherence/Anisotropy
            float coh = 0.0f;
            if (l1 + l2 > 1e-6f) {
                coh = (l1 - l2) / (l1 + l2);
            }

            size_t idx = static_cast<size_t>(y * w + x);
            field.angles[idx] = theta;
            field.coherence[idx] = coh;
            field.magnitude[idx] = std::sqrt(l1);
        }
    }

    return field;
}

} // namespace ArtifactCore
