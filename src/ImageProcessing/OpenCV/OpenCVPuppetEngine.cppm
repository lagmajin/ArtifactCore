module;

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

module ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;

namespace ArtifactCore {

class OpenCVPuppetEngine::Impl {
public:
    std::map<std::string, PuppetPin> pins;
    cv::Mat sourceImage;
    PuppetMesh initialMesh;
    PuppetMesh deformedMesh;

    void calculateMLSDeformation() {
        if (pins.empty() || initialMesh.vertices.empty()) {
            deformedMesh = initialMesh;
            return;
        }

        std::vector<cv::Point2f> p; // オリジナルのポジション
        std::vector<cv::Point2f> q; // 変形後のポジション
        std::vector<float> pinWeights; // 個別のウェイト
        
        std::vector<PuppetPin> overlapPins;

        for (const auto& kv : pins) {
            PuppetPin pin = kv.second;
            if (pin.type == PuppetPinType::Overlap) {
                overlapPins.push_back(pin);
                continue; // ジオメトリ変形(MLS)には直接参加せず、後で深度計算に使う
            }

            if (pin.type == PuppetPinType::Starch) {
                // Starchは強制的に元の位置を維持し、影響力を高くする(剛性)
                pin.currentPosition = pin.originalPosition;
                p.push_back(pin.originalPosition);
                q.push_back(pin.currentPosition);
                pinWeights.push_back(pin.weight * 50.0f); // 強い剛性
            } 
            else if (pin.type == PuppetPinType::Bend) {
                // Bendは仮想コントロールポイントを周辺に生成して回転を強制する
                p.push_back(pin.originalPosition);
                q.push_back(pin.currentPosition);
                pinWeights.push_back(pin.weight);

                float radius = 20.0f * pin.weight; // 回転の影響範囲
                float angle = pin.rotation;
                
                // 上下左右に4つの仮想ピンを置き、回転後の位置を指定
                cv::Point2f offsets[4] = { {radius, 0}, {-radius, 0}, {0, radius}, {0, -radius} };
                for (int i=0; i<4; i++) {
                    cv::Point2f orig = pin.originalPosition + offsets[i];
                    
                    float c = std::cos(angle);
                    float s = std::sin(angle);
                    cv::Point2f rotatedOffset(
                        offsets[i].x * c - offsets[i].y * s,
                        offsets[i].x * s + offsets[i].y * c
                    );
                    cv::Point2f cur = pin.currentPosition + rotatedOffset;
                    
                    p.push_back(orig);
                    q.push_back(cur);
                    pinWeights.push_back(pin.weight * 0.5f);
                }
            } 
            else {
                // 通常の Position ピン
                p.push_back(pin.originalPosition);
                q.push_back(pin.currentPosition);
                pinWeights.push_back(pin.weight);
            }
        }

        int npins = p.size();
        deformedMesh = initialMesh;

        // 深度(Z-Depth)の計算: IDW (Inverse Distance Weighting)
        if (!overlapPins.empty()) {
            for (size_t i = 0; i < initialMesh.vertices.size(); ++i) {
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
            }
        } else {
            for (size_t i = 0; i < deformedMesh.zDepth.size(); ++i) {
                deformedMesh.zDepth[i] = 0.0f;
            }
        }

        if (npins == 0) return;

        // Moving Least Squares (剛体を保つSimilitude変形)
        for (size_t i = 0; i < initialMesh.vertices.size(); ++i) {
            cv::Point2f v = initialMesh.vertices[i];
            
            std::vector<float> w(npins);
            float sum_w = 0;
            cv::Point2f p_star(0, 0), q_star(0, 0);
            
            bool is_control_point = false;
            for (int k = 0; k < npins; ++k) {
                float dist2 = std::pow(v.x - p[k].x, 2) + std::pow(v.y - p[k].y, 2);
                if (dist2 < 1e-4) { // ピンの直上
                    deformedMesh.vertices[i] = q[k];
                    is_control_point = true;
                    break;
                }
                w[k] = pinWeights[k] / (dist2 + 1e-8f); 
                sum_w += w[k];
            }
            if (is_control_point) continue;

            for (int k = 0; k < npins; ++k) {
                w[k] /= sum_w;
                p_star += p[k] * w[k];
                q_star += q[k] * w[k];
            }

            float sum_p_hat_sq = 0;
            for (int k = 0; k < npins; ++k) {
                cv::Point2f p_hat = p[k] - p_star;
                sum_p_hat_sq += w[k] * (p_hat.x * p_hat.x + p_hat.y * p_hat.y);
            }

            cv::Point2f v_hat = v - p_star;
            cv::Point2f new_v = q_star;

            if (sum_p_hat_sq > 1e-6) {
                float a = 0, b = 0;
                for (int k = 0; k < npins; ++k) {
                    cv::Point2f p_hat = p[k] - p_star;
                    cv::Point2f q_hat = q[k] - q_star;
                    a += w[k] * (p_hat.x * q_hat.x + p_hat.y * q_hat.y);
                    b += w[k] * (p_hat.x * q_hat.y - p_hat.y * q_hat.x);
                }
                float mu = sum_p_hat_sq;
                new_v.x += (a * v_hat.x - b * v_hat.y) / mu;
                new_v.y += (b * v_hat.x + a * v_hat.y) / mu;
            } else {
                new_v += v_hat;
            }
            
            deformedMesh.vertices[i] = new_v;
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

void OpenCVPuppetEngine::addPin(const PuppetPin& pin) {
    impl_->pins[pin.id] = pin;
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

void OpenCVPuppetEngine::reset() {
    impl_->pins.clear();
    impl_->deformedMesh = impl_->initialMesh;
}

} // namespace ArtifactCore
