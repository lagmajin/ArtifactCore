module;
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>
#include <algorithm>

module ImageProcessing;

import :NeuromorphicDisplace;

namespace ArtifactCore {

void NeuromorphicDisplace::process(ImageF32x4_RGBA& image, const NeuromorphicDisplaceSettings& settings) {
    if (image.isEmpty()) return;

    cv::Mat srcMat = image.toCVMat(); // CV_32FC4, BGRA
    const int w = srcMat.cols;
    const int h = srcMat.rows;

    // 1. Generate Heightmap (luminance of original image)
    cv::Mat gray;
    cv::cvtColor(srcMat, gray, cv::COLOR_BGRA2GRAY);

    // Apply softness (Gaussian blur) to round the edges of the heightfield
    if (settings.softness > 0.0f) {
        int ksize = static_cast<int>(std::round(settings.softness * 3.0f)) * 2 + 1;
        cv::GaussianBlur(gray, gray, cv::Size(ksize, ksize), settings.softness);
    }

    // 2. Compute normal vectors using Sobel gradients
    cv::Mat gradX, gradY;
    cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);

    // Calculate 3D light vector
    const float angleRad = settings.lightAngle * 3.14159265f / 180.0f;
    const float elevationRad = settings.lightElevation * 3.14159265f / 180.0f;

    // L = (Lx, Ly, Lz) pointing towards the light
    const float Lx = std::cos(elevationRad) * std::cos(angleRad);
    const float Ly = std::cos(elevationRad) * std::sin(angleRad);
    const float Lz = std::sin(elevationRad);

    cv::Mat dstMat = cv::Mat::zeros(h, w, CV_32FC4);

    // 3. Loop over all pixels to perform displacement and lighting
    for (int y = 0; y < h; ++y) {
        const float* pGradX = gradX.ptr<float>(y);
        const float* pGradY = gradY.ptr<float>(y);
        cv::Vec4f* dstRow = dstMat.ptr<cv::Vec4f>(y);

        for (int x = 0; x < w; ++x) {
            // Get gradient derivatives (scaled by depth)
            float dx = pGradX[x] * settings.depth;
            float dy = pGradY[x] * settings.depth;

            // Surface normal N = (-dx, -dy, 1) normalized
            float nx = -dx;
            float ny = -dy;
            float nz = 1.0f;
            float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nLen > 0.0f) {
                nx /= nLen;
                ny /= nLen;
                nz /= nLen;
            }

            // A. Refraction: offset sample coordinates along the normal vector
            float sx = x + nx * settings.refraction;
            float sy = y + ny * settings.refraction;

            // Clamp coordinates to bounds
            float cx = std::clamp(sx, 0.0f, static_cast<float>(w - 1));
            float cy = std::clamp(sy, 0.0f, static_cast<float>(h - 1));

            // Bilinear sampling of the displaced color
            int x0 = static_cast<int>(std::floor(cx));
            int x1 = std::min(x0 + 1, w - 1);
            int y0 = static_cast<int>(std::floor(cy));
            int y1 = std::min(y0 + 1, h - 1);

            float w_dx = cx - x0;
            float w_dy = cy - y0;

            const cv::Vec4f& p00 = srcMat.at<cv::Vec4f>(y0, x0);
            const cv::Vec4f& p10 = srcMat.at<cv::Vec4f>(y0, x1);
            const cv::Vec4f& p01 = srcMat.at<cv::Vec4f>(y1, x0);
            const cv::Vec4f& p11 = srcMat.at<cv::Vec4f>(y1, x1);

            cv::Vec4f refractColor = p00 * ((1.0f - w_dx) * (1.0f - w_dy)) +
                                     p10 * (w_dx * (1.0f - w_dy)) +
                                     p01 * ((1.0f - w_dx) * w_dy) +
                                     p11 * (w_dx * w_dy);

            // B. Lighting computation (Phong shading)
            // Diffuse term: N . L
            float dotNL = nx * Lx + ny * Ly + nz * Lz;
            float diffuseTerm = std::max(0.0f, dotNL);

            // Reflection vector R = 2 * (N . L) * N - L
            float rx = 2.0f * dotNL * nx - Lx;
            float ry = 2.0f * dotNL * ny - Ly;
            float rz = 2.0f * dotNL * nz - Lz;

            // Normalize R
            float rLen = std::sqrt(rx * rx + ry * ry + rz * rz);
            if (rLen > 0.0f) {
                rx /= rLen;
                ry /= rLen;
                rz /= rLen;
            }

            // View vector V = (0, 0, 1) pointing straight towards screen
            // Specular term: (R . V) ^ shininess = Rz ^ shininess
            float dotRV = std::max(0.0f, rz);
            float specularTerm = std::pow(dotRV, settings.shininess);

            // Lighting intensity (ambient + diffuse)
            float lightIntensity = settings.ambient + settings.diffuse * diffuseTerm;

            // Final Shaded Color (refractColor is in BGRA format)
            float b = refractColor[0] * lightIntensity + settings.specular * specularTerm;
            float g = refractColor[1] * lightIntensity + settings.specular * specularTerm;
            float r = refractColor[2] * lightIntensity + settings.specular * specularTerm;
            float a = refractColor[3]; // preserve alpha transparency

            cv::Vec4f shadedColor(
                std::clamp(b, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(r, 0.0f, 1.0f),
                a
            );

            // Blend with original input
            const cv::Vec4f& original = srcMat.at<cv::Vec4f>(y, x);
            dstRow[x] = original * (1.0f - settings.blend) + shadedColor * settings.blend;
        }
    }

    image.setFromCVMat(dstMat);
}

} // namespace ArtifactCore
