module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

module Core.Mask.RotoMask;

import std;

namespace ArtifactCore {

// ============================================================================
// 内部データ構造
// ============================================================================

/// 時間付きキーフレーム値
template<typename T>
struct KeyframeValue {
    double time;
    T value;
    RotoMask::Interpolation interpolation = RotoMask::Interpolation::Bezier;
    
    bool operator<(const KeyframeValue& other) const {
        return time < other.time;
    }
};

/// アニメーション対応プロパティ
template<typename T>
class AnimatedValue {
public:
    void setValue(const T& val, double time) {
        // 既存のキーフレームを探す
        for (auto& kf : keyframes_) {
            if (std::abs(kf.time - time) < 0.0001) {
                kf.value = val;
                return;
            }
        }
        // 新しいキーフレームを追加
        keyframes_.push_back({time, val});
        std::sort(keyframes_.begin(), keyframes_.end());
    }
    
    T valueAt(double time) const {
        if (keyframes_.empty()) return defaultValue_;
        if (keyframes_.size() == 1) return keyframes_[0].value;
        
        // 境界外
        if (time <= keyframes_.front().time) return keyframes_.front().value;
        if (time >= keyframes_.back().time) return keyframes_.back().value;
        
        // 補間
        for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
            if (time >= keyframes_[i].time && time <= keyframes_[i + 1].time) {
                double t = (time - keyframes_[i].time) / 
                          (keyframes_[i + 1].time - keyframes_[i].time);
                
                switch (keyframes_[i].interpolation) {
                    case RotoMask::Interpolation::Step:
                        return keyframes_[i].value;
                    case RotoMask::Interpolation::Linear:
                        return interpolateLinear(keyframes_[i].value, keyframes_[i + 1].value, t);
                    case RotoMask::Interpolation::Bezier:
                    default:
                        return interpolateLinear(keyframes_[i].value, keyframes_[i + 1].value, t);
                }
            }
        }
        return keyframes_.back().value;
    }
    
    bool hasKeyframes() const { return !keyframes_.empty(); }
    bool isAnimated() const { return keyframes_.size() > 1; }
    
    std::vector<double> keyframeTimes() const {
        std::vector<double> times;
        for (const auto& kf : keyframes_) {
            times.push_back(kf.time);
        }
        return times;
    }
    
    void setDefaultValue(const T& val) { defaultValue_ = val; }
    const T& defaultValue() const { return defaultValue_; }
    
    void setInterpolation(double time, RotoMask::Interpolation interp) {
        for (auto& kf : keyframes_) {
            if (std::abs(kf.time - time) < 0.0001) {
                kf.interpolation = interp;
                return;
            }
        }
    }
    
private:
    T defaultValue_{};
    std::vector<KeyframeValue<T>> keyframes_;
    
    T interpolateLinear(const T& a, const T& b, double t) const {
        return a + (b - a) * t;
    }
};

/// 頂点データ
struct VertexData {
    int id;
    AnimatedValue<QPointF> position;
    AnimatedValue<QPointF> inTangent;
    AnimatedValue<QPointF> outTangent;
    RotoMask::Interpolation interpolation = RotoMask::Interpolation::Bezier;
};

// ============================================================================
// RotoMask::Impl
// ============================================================================

class RotoMask::Impl {
public:
    std::map<int, VertexData> vertices;
    int nextVertexID = 1;
    
    bool closed = true;
    bool inverted = false;
    QString name;
    RotoMaskMode mode = RotoMaskMode::Add;
    
    AnimatedValue<float> opacity;
    AnimatedValue<float> feather;
    AnimatedValue<float> expansion;
    
    Impl() {
        opacity.setDefaultValue(1.0f);
        feather.setDefaultValue(0.0f);
        expansion.setDefaultValue(0.0f);
    }
    
    // ベジェ曲線の評価
    static QPointF cubicBezier(const QPointF& p0, const QPointF& p1,
                               const QPointF& p2, const QPointF& p3, float t) {
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;
        return uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
    }
    
    // パスからポリゴン生成
    std::vector<QPointF> toPolygon(const std::vector<RotoVertex>& verts, int subdivisions = 16) const {
        std::vector<QPointF> poly;
        const int n = static_cast<int>(verts.size());
        if (n < 2) {
            for (const auto& v : verts) poly.push_back(v.position);
            return poly;
        }
        
        int segments = closed ? n : n - 1;
        for (int i = 0; i < segments; ++i) {
            const auto& v0 = verts[i];
            const auto& v1 = verts[(i + 1) % n];
            QPointF cp0 = v0.position + v0.outTangent;
            QPointF cp1 = v1.position + v1.inTangent;
            for (int s = 0; s < subdivisions; ++s) {
                float t = static_cast<float>(s) / static_cast<float>(subdivisions);
                poly.push_back(cubicBezier(v0.position, cp0, cp1, v1.position, t));
            }
        }
        if (!closed && n > 0) {
            poly.push_back(verts.back().position);
        }
        return poly;
    }
};

// ============================================================================
// RotoMask 実装
// ============================================================================

RotoMask::RotoMask() : impl_(new Impl()) {}

RotoMask::~RotoMask() { delete impl_; }

RotoMask::RotoMask(const RotoMask& other) : impl_(new Impl(*other.impl_)) {}

RotoMask& RotoMask::operator=(const RotoMask& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
    }
    return *this;
}

RotoMask::RotoMask(RotoMask&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

RotoMask& RotoMask::operator=(RotoMask&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

// ========================================
// 頂点操作
// ========================================

RotoMask::VertexID RotoMask::addVertex(const QPointF& position) {
    return addVertex(position, QPointF(0, 0), QPointF(0, 0));
}

RotoMask::VertexID RotoMask::addVertex(const QPointF& position, 
                                        const QPointF& inTangent, 
                                        const QPointF& outTangent) {
    VertexData vd;
    vd.id = impl_->nextVertexID++;
    vd.position.setDefaultValue(position);
    vd.inTangent.setDefaultValue(inTangent);
    vd.outTangent.setDefaultValue(outTangent);
    impl_->vertices[vd.id] = vd;
    return vd.id;
}

void RotoMask::removeVertex(VertexID id) {
    impl_->vertices.erase(id);
}

void RotoMask::clearVertices() {
    impl_->vertices.clear();
}

int RotoMask::vertexCount() const {
    return static_cast<int>(impl_->vertices.size());
}

std::vector<RotoMask::VertexID> RotoMask::vertexIDs() const {
    std::vector<VertexID> ids;
    for (const auto& pair : impl_->vertices) {
        ids.push_back(pair.first);
    }
    return ids;
}

// ========================================
// 頂点位置・制御点
// ========================================

void RotoMask::setVertexPosition(VertexID id, const QPointF& position, double time) {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        it->second.position.setValue(position, time);
    }
}

QPointF RotoMask::vertexPosition(VertexID id, double time) const {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        return it->second.position.valueAt(time);
    }
    return QPointF();
}

void RotoMask::setInTangent(VertexID id, const QPointF& tangent, double time) {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        it->second.inTangent.setValue(tangent, time);
    }
}

QPointF RotoMask::inTangent(VertexID id, double time) const {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        return it->second.inTangent.valueAt(time);
    }
    return QPointF();
}

void RotoMask::setOutTangent(VertexID id, const QPointF& tangent, double time) {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        it->second.outTangent.setValue(tangent, time);
    }
}

QPointF RotoMask::outTangent(VertexID id, double time) const {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        return it->second.outTangent.valueAt(time);
    }
    return QPointF();
}

RotoVertex RotoMask::getVertex(VertexID id, double time) const {
    return RotoVertex(
        vertexPosition(id, time),
        inTangent(id, time),
        outTangent(id, time)
    );
}

std::vector<RotoVertex> RotoMask::sampleVertices(double time) const {
    std::vector<RotoVertex> result;
    // ID順にソート（追加順を維持）
    std::vector<std::pair<int, VertexData>> sorted(impl_->vertices.begin(), impl_->vertices.end());
    std::sort(sorted.begin(), sorted.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    for (const auto& pair : sorted) {
        result.push_back(RotoVertex(
            pair.second.position.valueAt(time),
            pair.second.inTangent.valueAt(time),
            pair.second.outTangent.valueAt(time)
        ));
    }
    return result;
}

// ========================================
// パス属性
// ========================================

bool RotoMask::isClosed() const { return impl_->closed; }
void RotoMask::setClosed(bool closed) { impl_->closed = closed; }

QString RotoMask::name() const { return impl_->name; }
void RotoMask::setName(const QString& name) { impl_->name = name; }

// ========================================
// マスク属性
// ========================================

RotoMaskMode RotoMask::mode() const { return impl_->mode; }
void RotoMask::setMode(RotoMaskMode mode) { impl_->mode = mode; }

void RotoMask::setOpacity(float opacity, double time) {
    impl_->opacity.setValue(std::clamp(opacity, 0.0f, 1.0f), time);
}

float RotoMask::opacity(double time) const {
    return impl_->opacity.valueAt(time);
}

void RotoMask::setFeather(float feather, double time) {
    impl_->feather.setValue(std::max(0.0f, feather), time);
}

float RotoMask::feather(double time) const {
    return impl_->feather.valueAt(time);
}

void RotoMask::setExpansion(float expansion, double time) {
    impl_->expansion.setValue(expansion, time);
}

float RotoMask::expansion(double time) const {
    return impl_->expansion.valueAt(time);
}

bool RotoMask::isInverted() const { return impl_->inverted; }
void RotoMask::setInverted(bool inverted) { impl_->inverted = inverted; }

// ========================================
// アニメーション
// ========================================

bool RotoMask::isAnimated() const {
    if (impl_->opacity.isAnimated() || impl_->feather.isAnimated() || impl_->expansion.isAnimated()) {
        return true;
    }
    for (const auto& pair : impl_->vertices) {
        if (pair.second.position.isAnimated() ||
            pair.second.inTangent.isAnimated() ||
            pair.second.outTangent.isAnimated()) {
            return true;
        }
    }
    return false;
}

std::vector<double> RotoMask::keyframeTimes() const {
    std::set<double> times;
    
    // 全プロパティからキーフレーム時刻を収集
    for (const auto& t : impl_->opacity.keyframeTimes()) times.insert(t);
    for (const auto& t : impl_->feather.keyframeTimes()) times.insert(t);
    for (const auto& t : impl_->expansion.keyframeTimes()) times.insert(t);
    
    for (const auto& pair : impl_->vertices) {
        for (const auto& t : pair.second.position.keyframeTimes()) times.insert(t);
        for (const auto& t : pair.second.inTangent.keyframeTimes()) times.insert(t);
        for (const auto& t : pair.second.outTangent.keyframeTimes()) times.insert(t);
    }
    
    return std::vector<double>(times.begin(), times.end());
}

double RotoMask::startTime() const {
    auto times = keyframeTimes();
    return times.empty() ? 0.0 : times.front();
}

double RotoMask::endTime() const {
    auto times = keyframeTimes();
    return times.empty() ? 0.0 : times.back();
}

void RotoMask::addKeyframe(double time) {
    // 現在の全プロパティ値をその時間にキーフレーム追加
    for (auto& pair : impl_->vertices) {
        pair.second.position.setValue(pair.second.position.valueAt(time), time);
        pair.second.inTangent.setValue(pair.second.inTangent.valueAt(time), time);
        pair.second.outTangent.setValue(pair.second.outTangent.valueAt(time), time);
    }
    impl_->opacity.setValue(impl_->opacity.valueAt(time), time);
    impl_->feather.setValue(impl_->feather.valueAt(time), time);
    impl_->expansion.setValue(impl_->expansion.valueAt(time), time);
}

void RotoMask::removeKeyframe(double time) {
    // 実装簡略化：各プロパティから該当時間のキーフレームを削除
    // 詳細実装では各AnimatedValueにremoveKeyframeメソッドを追加
}

// ========================================
// 補間設定
// ========================================

void RotoMask::setPositionInterpolation(VertexID id, Interpolation interp) {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        it->second.interpolation = interp;
    }
}

RotoMask::Interpolation RotoMask::positionInterpolation(VertexID id) const {
    auto it = impl_->vertices.find(id);
    if (it != impl_->vertices.end()) {
        return it->second.interpolation;
    }
    return Interpolation::Bezier;
}

// ========================================
// ラスタライズ
// ========================================

void RotoMask::rasterize(double time, int width, int height, float* outData) const {
    // 初期化
    std::fill(outData, outData + width * height, 0.0f);
    
    auto verts = sampleVertices(time);
    if (verts.empty()) return;
    
    // ポリゴン生成
    auto poly = impl_->toPolygon(verts, 16);
    if (poly.empty()) return;
    
    // 簡易的なラスタライズ（スキャンライン）
    // 実際の実装ではOpenCVなどを使用
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // ポイントインポリゴンテスト
            bool inside = false;
            int n = static_cast<int>(poly.size());
            for (int i = 0, j = n - 1; i < n; j = i++) {
                if (((poly[i].y() > y) != (poly[j].y() > y)) &&
                    (x < (poly[j].x() - poly[i].x()) * (y - poly[i].y()) / 
                          (poly[j].y() - poly[i].y()) + poly[i].x())) {
                    inside = !inside;
                }
            }
            outData[y * width + x] = inside ? 1.0f : 0.0f;
        }
    }
    
    // フェザー適用（簡易）
    float f = feather(time);
    if (f > 0.5f) {
        // ガウシアンブラーを適用（簡易実装）
        // 実際は別途実装
    }
    
    // 拡張/収縮
    float exp = expansion(time);
    if (std::abs(exp) > 0.5f) {
        // モルフォロジー演算
    }
    
    // 不透明度適用
    float op = opacity(time);
    if (op < 1.0f) {
        for (int i = 0; i < width * height; ++i) {
            outData[i] *= op;
        }
    }
    
    // 反転
    if (impl_->inverted) {
        for (int i = 0; i < width * height; ++i) {
            outData[i] = 1.0f - outData[i];
        }
    }
}

QRectF RotoMask::boundingBox(double time) const {
    auto verts = sampleVertices(time);
    if (verts.empty()) return QRectF();
    
    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::min();
    qreal maxY = std::numeric_limits<qreal>::min();
    
    for (const auto& v : verts) {
        minX = std::min(minX, v.position.x());
        minY = std::min(minY, v.position.y());
        maxX = std::max(maxX, v.position.x());
        maxY = std::max(maxY, v.position.y());
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

// ========================================
// ユーティリティ
// ========================================

RotoMask RotoMask::clone() const {
    return RotoMask(*this);
}

void RotoMask::copyKeyframesFrom(const RotoMask& other, double timeOffset) {
    // 簡易実装
    Q_UNUSED(other);
    Q_UNUSED(timeOffset);
}

void RotoMask::reverse() {
    // 頂点順序を反転
    std::vector<std::pair<int, VertexData>> sorted(impl_->vertices.begin(), impl_->vertices.end());
    std::reverse(sorted.begin(), sorted.end());
    
    // タンジェントも反転
    for (auto& pair : sorted) {
        auto inT = pair.second.inTangent;
        pair.second.inTangent = pair.second.outTangent;
        pair.second.outTangent = inT;
    }
    
    // 新しいIDで再構築
    impl_->vertices.clear();
    int newId = 0;
    for (auto& pair : sorted) {
        pair.first = newId++;
        impl_->vertices[pair.first] = pair.second;
    }
}

} // namespace ArtifactCore