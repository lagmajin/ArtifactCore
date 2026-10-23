module;
#include <utility>

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "../../../include/Define/DllExportMacro.hpp"

module ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;

import Core.Parallel;

namespace ArtifactCore {

LIBRARY_DLL_API std::vector<PuppetConstraint> buildPuppetConstraints(
    const std::vector<PuppetPin>& pins)
{
    std::vector<PuppetConstraint> constraints;
    for (const PuppetPin& pin : pins) {
        if (pin.type == PuppetPinType::Overlap) {
            continue; // 深さ計算専用なので MLS には参加させない
        }
        if (pin.type == PuppetPinType::Starch) {
            // Starch は現在位置を元座標へ留めて 주변の剛性を上げる。
            constraints.push_back({pin.originalPosition, pin.originalPosition,
                                    pin.weight * 50.0f});
            continue;
        }
        constraints.push_back({pin.originalPosition, pin.currentPosition,
                                pin.weight});
        if (pin.type != PuppetPinType::Bend) {
            continue;
        }
        // Bend は回転を強制するため、周辺 4 点へ仮想拘束点を立てる。
        const float radius = 20.0f * pin.weight;
        const float cosine = std::cos(pin.rotation);
        const float sine = std::sin(pin.rotation);
        const cv::Point2f offsets[4] = {{radius, 0.0f}, {-radius, 0.0f},
                                        {0.0f, radius}, {0.0f, -radius}};
        for (const cv::Point2f& offset : offsets) {
            const cv::Point2f rotated(offset.x * cosine - offset.y * sine,
                                       offset.x * sine + offset.y * cosine);
            constraints.push_back(
                {pin.originalPosition + offset, pin.currentPosition + rotated,
                 pin.weight * 0.5f});
        }
    }
    return constraints;
}

LIBRARY_DLL_API cv::Point2f evaluatePuppetMLS(
    const std::vector<PuppetConstraint>& constraints,
                              const cv::Point2f& samplePoint)
{
    if (constraints.empty()) {
        return samplePoint;
    }
    float sumWeight = 0.0f;
    cv::Point2f pStar(0.0f, 0.0f);
    cv::Point2f qStar(0.0f, 0.0f);
    for (const PuppetConstraint& c : constraints) {
        const float dx = samplePoint.x - c.source.x;
        const float dy = samplePoint.y - c.source.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < 1e-4f) {
            return c.target; // 制御点そのもの
        }
        const float w = c.weight / (distanceSquared + 1e-8f);
        sumWeight += w;
        pStar += c.source * w;
        qStar += c.target * w;
    }
    if (!(sumWeight > 0.0f) || !std::isfinite(sumWeight)) {
        return samplePoint;
    }
    const float inverseWeightSum = 1.0f / sumWeight;
    pStar *= inverseWeightSum;
    qStar *= inverseWeightSum;

    float mu = 0.0f;
    for (const PuppetConstraint& c : constraints) {
        const float dx = samplePoint.x - c.source.x;
        const float dy = samplePoint.y - c.source.y;
        const float w =
            (c.weight / (dx * dx + dy * dy + 1e-8f)) * inverseWeightSum;
        const cv::Point2f pHat = c.source - pStar;
        mu += w * (pHat.x * pHat.x + pHat.y * pHat.y);
    }

    const cv::Point2f vHat = samplePoint - pStar;
    cv::Point2f mapped = qStar;
    if (mu > 1e-6f && std::isfinite(mu)) {
        float a = 0.0f;
        float b = 0.0f;
        for (const PuppetConstraint& c : constraints) {
            const float dx = samplePoint.x - c.source.x;
            const float dy = samplePoint.y - c.source.y;
            const float w =
                (c.weight / (dx * dx + dy * dy + 1e-8f)) * inverseWeightSum;
            const cv::Point2f pHat = c.source - pStar;
            const cv::Point2f qHat = c.target - qStar;
            a += w * (pHat.x * qHat.x + pHat.y * qHat.y);
            b += w * (pHat.x * qHat.y - pHat.y * qHat.x);
        }
        mapped.x += (a * vHat.x - b * vHat.y) / mu;
        mapped.y += (b * vHat.x + a * vHat.y) / mu;
    } else {
        mapped += vHat;
    }
    if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y)) {
        return samplePoint;
    }
    return mapped;
}

class OpenCVPuppetEngine::Impl {
public:
    std::map<std::string, PuppetPin> pins;
    cv::Mat sourceImage;
    PuppetMesh initialMesh;
    PuppetMesh deformedMesh;
    std::vector<PuppetPin> overlapPins;
    std::vector<cv::Point2f> initialGridVertices;
    std::vector<cv::Point2f> deformedGridVertices;
    int gridColumns = 0;
    int gridRows = 0;

    void calculateMLSDeformation() {
        if (pins.empty() || initialMesh.vertices.empty()) {
            deformedMesh = initialMesh;
            return;
        }

        overlapPins.clear();

        std::vector<PuppetPin> mlsPins;
        mlsPins.reserve(pins.size());
        for (const auto& kv : pins) {
            if (kv.second.type == PuppetPinType::Overlap) {
                overlapPins.push_back(kv.second);
                continue; // ジオメトリ変形(MLS)には直接参加せず、後で深度計算に使う
            }
            mlsPins.push_back(kv.second);
        }
        // 拘束点生成規則はシェイプ経路と共有し、両者の絵を一致させる。
        const std::vector<PuppetConstraint> constraints =
            buildPuppetConstraints(mlsPins);

        deformedMesh = initialMesh;

        // 深度(Z-Depth)の計算: IDW (Inverse Distance Weighting)
        if (!overlapPins.empty()) {
            Parallel::ForSize(0, initialMesh.vertices.size(),
                              initialMesh.vertices.size(), [&](std::size_t i) {
                cv::Point2f v = initialMesh.vertices[i];
                float depth_sum = 0.0f;
                float w_sum = 0.0f;
                for (const auto& op : overlapPins) {
                    float distSq = std::pow(v.x - op.originalPosition.x, 2) + std::pow(v.y - op.originalPosition.y, 2);
                    float w = op.weight / (distSq + 1.0f); 
                    depth_sum += op.depth * w;
                    w_sum += w;
                }
                deformedMesh.zDepth[i] = w_sum > 0 ? (depth_sum / w_sum) : 0.0f;
            });
        } else {
            Parallel::ForSize(0, deformedMesh.zDepth.size(),
                              deformedMesh.zDepth.size(), [&](std::size_t i) {
                deformedMesh.zDepth[i] = 0.0f;
            });
        }

        // Moving Least Squares (剛体を保つSimilitude変形)
        // 写像式は evaluatePuppetMLS に集約し、シェイプ経路と同じ式を使う。
        Parallel::ForSize(0, initialMesh.vertices.size(),
                          initialMesh.vertices.size(), [&](std::size_t i) {
            deformedMesh.vertices[i] =
                evaluatePuppetMLS(constraints, initialMesh.vertices[i]);
        });
    }

    void calculateGridDeformation() {
        deformedMesh = initialMesh;
        if (gridColumns < 2 || gridRows < 2 ||
            initialGridVertices.size() != deformedGridVertices.size() ||
            deformedGridVertices.size() !=
                static_cast<size_t>(gridColumns * gridRows)) return;

        const float maxX = static_cast<float>(std::max(1, sourceImage.cols));
        const float maxY = static_cast<float>(std::max(1, sourceImage.rows));
        for (size_t i = 0; i < initialMesh.vertices.size(); ++i) {
            const cv::Point2f source = initialMesh.vertices[i];
            const float gridX = std::clamp(
                source.x / maxX * static_cast<float>(gridColumns - 1),
                0.0f, static_cast<float>(gridColumns - 1));
            const float gridY = std::clamp(
                source.y / maxY * static_cast<float>(gridRows - 1),
                0.0f, static_cast<float>(gridRows - 1));
            const int column = std::min(static_cast<int>(gridX), gridColumns - 2);
            const int row = std::min(static_cast<int>(gridY), gridRows - 2);
            const float tx = gridX - static_cast<float>(column);
            const float ty = gridY - static_cast<float>(row);
            const size_t topLeft = static_cast<size_t>(row * gridColumns + column);
            const size_t topRight = topLeft + 1;
            const size_t bottomLeft = topLeft + static_cast<size_t>(gridColumns);
            const size_t bottomRight = bottomLeft + 1;
            const cv::Point2f top =
                deformedGridVertices[topLeft] * (1.0f - tx) +
                deformedGridVertices[topRight] * tx;
            const cv::Point2f bottom =
                deformedGridVertices[bottomLeft] * (1.0f - tx) +
                deformedGridVertices[bottomRight] * tx;
            deformedMesh.vertices[i] = top * (1.0f - ty) + bottom * ty;
        }
    }
};

OpenCVPuppetEngine::OpenCVPuppetEngine() : impl_(std::make_unique<Impl>()) {}
OpenCVPuppetEngine::~OpenCVPuppetEngine() = default;

void OpenCVPuppetEngine::bindImage(const cv::Mat& sourceImage, int detailLevel) {
    if (sourceImage.empty()) return;
    impl_->sourceImage = sourceImage.clone();
    impl_->initialMesh.vertices.clear();
    impl_->initialMesh.indices.clear();
    impl_->initialMesh.texCoords.clear();
    impl_->initialMesh.zDepth.clear();
    impl_->pins.clear();
    impl_->initialGridVertices.clear();
    impl_->deformedGridVertices.clear();
    impl_->gridColumns = 0;
    impl_->gridRows = 0;

    // アルファチャンネルで輪郭抽出
    cv::Mat mask;
    if (sourceImage.channels() == 4) {
        cv::extractChannel(sourceImage, mask, 3);
    } else {
        cv::cvtColor(sourceImage, mask, cv::COLOR_BGR2GRAY);
        cv::threshold(mask, mask, 1, 255, cv::THRESH_BINARY);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return;

    // 最大の輪郭を抽出
    size_t maxIdx = 0;
    double maxArea = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = i;
        }
    }

    // 輪郭を単純化してポイントに追加
    std::vector<cv::Point2f> simplified;
    double epsilon = detailLevel > 0 ? (100.0 / detailLevel) : 10.0;
    std::vector<cv::Point> approx;
    cv::approxPolyDP(contours[maxIdx], approx, epsilon, true);
    for (const auto& p : approx) {
        simplified.push_back(cv::Point2f((float)p.x, (float)p.y));
    }

    // メッシュ用の内部制御グリッド点を追加
    cv::Rect bound = cv::boundingRect(contours[maxIdx]);
    int step = detailLevel > 0 ? std::max(10, 200 / detailLevel) : 20;
    for (int y = bound.y; y < bound.y + bound.height; y += step) {
        for (int x = bound.x; x < bound.x + bound.width; x += step) {
            cv::Point2f pt2f((float)x, (float)y);
            if (cv::pointPolygonTest(simplified, pt2f, false) >= 0) {
                simplified.push_back(pt2f);
            }
        }
    }

    // ドロネー分割
    cv::Subdiv2D subdiv(cv::Rect(0, 0, sourceImage.cols, sourceImage.rows));
    for (const auto& p : simplified) {
        subdiv.insert(p);
    }

    std::vector<cv::Vec6f> triangleList;
    subdiv.getTriangleList(triangleList);

    std::vector<cv::Point2f>& pts = impl_->initialMesh.vertices;
    std::map<std::pair<float, float>, int> pt2idx;
    auto getIdx = [&](const cv::Point2f& ppt) -> int {
        std::pair<float, float> key = {ppt.x, ppt.y};
        if (pt2idx.count(key) == 0) {
            pt2idx[key] = pts.size();
            pts.push_back(ppt);
        }
        return pt2idx[key];
    };

    for (const auto& t : triangleList) {
        cv::Point2f pt[3] = {
            cv::Point2f(t[0], t[1]),
            cv::Point2f(t[2], t[3]),
            cv::Point2f(t[4], t[5])
        };
        // 重心がポリゴン内に含まれるものだけメッシュにする
        cv::Point2f center = (pt[0] + pt[1] + pt[2]) * (1.0f/3.0f);
        if (center.x >= 0 && center.x < sourceImage.cols && center.y >= 0 && center.y < sourceImage.rows) {
            if (cv::pointPolygonTest(simplified, center, false) >= 0) {
                impl_->initialMesh.indices.push_back(getIdx(pt[0]));
                impl_->initialMesh.indices.push_back(getIdx(pt[1]));
                impl_->initialMesh.indices.push_back(getIdx(pt[2]));
            }
        }
    }
    
    // UI/GPUレンダリング用にUVを正規化して保存し、深度バッファを初期化
    for (const auto& p : impl_->initialMesh.vertices) {
        impl_->initialMesh.texCoords.push_back(cv::Point2f(p.x / sourceImage.cols, p.y / sourceImage.rows));
        impl_->initialMesh.zDepth.push_back(0.0f);
    }

    impl_->deformedMesh = impl_->initialMesh;
}

bool OpenCVPuppetEngine::configureGrid(int columns, int rows) {
    if (impl_->sourceImage.empty()) return false;
    const int safeColumns = std::clamp(columns, 2, 64);
    const int safeRows = std::clamp(rows, 2, 64);
    impl_->gridColumns = safeColumns;
    impl_->gridRows = safeRows;
    const size_t count = static_cast<size_t>(safeColumns) *
                         static_cast<size_t>(safeRows);
    impl_->initialGridVertices.resize(count);
    impl_->deformedGridVertices.resize(count);
    for (int row = 0; row < safeRows; ++row) {
        for (int column = 0; column < safeColumns; ++column) {
            const size_t index = static_cast<size_t>(row * safeColumns + column);
            const cv::Point2f point(
                static_cast<float>(impl_->sourceImage.cols) * column /
                    static_cast<float>(safeColumns - 1),
                static_cast<float>(impl_->sourceImage.rows) * row /
                    static_cast<float>(safeRows - 1));
            impl_->initialGridVertices[index] = point;
            impl_->deformedGridVertices[index] = point;
        }
    }
    impl_->pins.clear();
    impl_->calculateGridDeformation();
    return true;
}

bool OpenCVPuppetEngine::setGridVertices(
    const std::vector<cv::Point2f>& vertices) {
    if (!hasGrid() || vertices.size() != impl_->deformedGridVertices.size()) {
        return false;
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!std::isfinite(vertices[i].x) || !std::isfinite(vertices[i].y)) {
            return false;
        }
    }
    impl_->deformedGridVertices = vertices;
    impl_->calculateGridDeformation();
    return true;
}

bool OpenCVPuppetEngine::hasGrid() const {
    return impl_->gridColumns >= 2 && impl_->gridRows >= 2 &&
           !impl_->initialGridVertices.empty();
}

int OpenCVPuppetEngine::gridColumns() const { return impl_->gridColumns; }
int OpenCVPuppetEngine::gridRows() const { return impl_->gridRows; }

const std::vector<cv::Point2f>& OpenCVPuppetEngine::gridVerticesView() const {
    return impl_->deformedGridVertices;
}

void OpenCVPuppetEngine::addPin(const PuppetPin& pin) {
    impl_->pins[pin.id] = pin;
    impl_->calculateMLSDeformation();
}

void OpenCVPuppetEngine::setPins(const std::vector<PuppetPin>& pins) {
    impl_->initialGridVertices.clear();
    impl_->deformedGridVertices.clear();
    impl_->gridColumns = 0;
    impl_->gridRows = 0;
    for (auto it = impl_->pins.begin(); it != impl_->pins.end();) {
        const bool retained = std::any_of(
            pins.begin(), pins.end(),
            [&it](const PuppetPin& pin) { return pin.id == it->first; });
        if (retained) ++it;
        else it = impl_->pins.erase(it);
    }
    for (const auto& pin : pins) {
        if (pin.id.empty()) continue;
        const auto existing = impl_->pins.find(pin.id);
        if (existing != impl_->pins.end()) existing->second = pin;
        else impl_->pins.emplace(pin.id, pin);
    }
    impl_->calculateMLSDeformation();
}

void OpenCVPuppetEngine::removePin(const std::string& pinId) {
    if (impl_->pins.erase(pinId)) {
        impl_->calculateMLSDeformation();
    }
}

void OpenCVPuppetEngine::updatePinPosition(const std::string& pinId, const cv::Point2f& newPosition) {
    auto it = impl_->pins.find(pinId);
    if (it != impl_->pins.end()) {
        it->second.currentPosition = newPosition;
        impl_->calculateMLSDeformation();
    }
}

std::vector<PuppetPin> OpenCVPuppetEngine::getPins() const {
    std::vector<PuppetPin> result;
    for (const auto& kv : impl_->pins) {
        result.push_back(kv.second);
    }
    return result;
}

cv::Mat OpenCVPuppetEngine::renderDeformedImage(PuppetDeformationMethod method) {
    if (impl_->sourceImage.empty() || impl_->deformedMesh.vertices.empty()) {
        return impl_->sourceImage;
    }
    
    // CPUベースの安直なパペットレンダラ (三角形ごとにアフィン変換) -> 重なりを考慮
    cv::Mat result = cv::Mat::zeros(impl_->sourceImage.size(), impl_->sourceImage.type());

    // オーバーラップ(深度)対応: 各三角形の平均深度を求め、奥(小さい値)から手前(大きい値)へ描画順をソート
    struct TriInfo {
        int idx0, idx1, idx2;
        float depth;
    };
    std::vector<TriInfo> triangles;
    for (size_t i = 0; i < impl_->deformedMesh.indices.size(); i += 3) {
        int idx1 = impl_->deformedMesh.indices[i];
        int idx2 = impl_->deformedMesh.indices[i+1];
        int idx3 = impl_->deformedMesh.indices[i+2];
        float avgDepth = (impl_->deformedMesh.zDepth[idx1] + impl_->deformedMesh.zDepth[idx2] + impl_->deformedMesh.zDepth[idx3]) / 3.0f;
        triangles.push_back({idx1, idx2, idx3, avgDepth});
    }

    std::sort(triangles.begin(), triangles.end(), [](const TriInfo& a, const TriInfo& b) {
        return a.depth < b.depth;
    });

    for (const auto& tri : triangles) {
        int idx1 = tri.idx0;
        int idx2 = tri.idx1;
        int idx3 = tri.idx2;

        cv::Point2f dp[3] = {
            impl_->deformedMesh.vertices[idx1],
            impl_->deformedMesh.vertices[idx2],
            impl_->deformedMesh.vertices[idx3]
        };

        cv::Point2f sp[3] = {
            impl_->initialMesh.vertices[idx1],
            impl_->initialMesh.vertices[idx2],
            impl_->initialMesh.vertices[idx3]
        };

        cv::Mat warpMat = cv::getAffineTransform(sp, dp);
        
        cv::Mat mask = cv::Mat::zeros(impl_->sourceImage.size(), CV_8UC1);
        std::vector<cv::Point> polyP = {
            cv::Point((int)dp[0].x, (int)dp[0].y),
            cv::Point((int)dp[1].x, (int)dp[1].y),
            cv::Point((int)dp[2].x, (int)dp[2].y)
        };
        cv::fillConvexPoly(mask, polyP.data(), 3, cv::Scalar(255));

        cv::Mat warpedTri;
        // BORDER_TRANSPARENT を使うと、既に描画された三角形の隙間を消さずにブレンドできる
        cv::warpAffine(impl_->sourceImage, warpedTri, warpMat, result.size(), cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
        warpedTri.copyTo(result, mask);
    }

    return result;
}

PuppetMesh OpenCVPuppetEngine::getDeformedMesh() const {
    return impl_->deformedMesh;
}

const PuppetMesh& OpenCVPuppetEngine::deformedMeshView() const {
    return impl_->deformedMesh;
}

void OpenCVPuppetEngine::reset() {
    impl_->pins.clear();
    impl_->initialGridVertices.clear();
    impl_->deformedGridVertices.clear();
    impl_->gridColumns = 0;
    impl_->gridRows = 0;
    impl_->deformedMesh = impl_->initialMesh;
}

} // namespace ArtifactCore
