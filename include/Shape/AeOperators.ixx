module;

#include <QObject>
#include <QJsonObject>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPointF>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <memory>
#include <vector>

export module Shape.AeOperators;

import Shape.Operator;
import Shape.Path;
import Shape.Types;

export namespace ArtifactCore {

namespace detail {

inline double length(const QPointF& v)
{
    return std::hypot(v.x(), v.y());
}

inline double distance(const QPointF& a, const QPointF& b)
{
    return length(b - a);
}

inline QPointF transformRadially(
    const QPointF& point,
    const QPointF& center,
    double maxRadius,
    double strength,
    double twistDegrees)
{
    QPointF delta = point - center;
    const double radius = length(delta);
    if (radius <= 1e-6 || maxRadius <= 1e-6) {
        return point;
    }

    const double normalized = std::clamp(radius / maxRadius, 0.0, 1.0);
    const double influence = 1.0 - normalized;

    double scale = 1.0 + strength * influence;
    if (scale < 0.0) {
        scale = 0.0;
    }

    QPointF warped = center + delta * scale;

    if (std::abs(twistDegrees) > 1e-6) {
        const double angle = twistDegrees * influence * (std::numbers::pi / 180.0);
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        const QPointF fromCenter = warped - center;
        warped = QPointF(
            center.x() + fromCenter.x() * c - fromCenter.y() * s,
            center.y() + fromCenter.x() * s + fromCenter.y() * c);
    }

    return warped;
}

inline double maxRadiusFromCenter(const ShapePath& path, const QPointF& center)
{
    double maxRadius = 0.0;
    for (const auto& command : path.commands()) {
        switch (command.type) {
        case PathCommandType::MoveTo:
        case PathCommandType::LineTo:
            maxRadius = std::max(maxRadius, distance(command.points[0], center));
            break;
        case PathCommandType::QuadTo:
            maxRadius = std::max(maxRadius, distance(command.points[0], center));
            maxRadius = std::max(maxRadius, distance(command.points[1], center));
            break;
        case PathCommandType::CubicTo:
            maxRadius = std::max(maxRadius, distance(command.points[0], center));
            maxRadius = std::max(maxRadius, distance(command.points[1], center));
            maxRadius = std::max(maxRadius, distance(command.points[2], center));
            break;
        case PathCommandType::Close:
            break;
        }
    }
    return maxRadius;
}

inline ShapePath warpPath(
    const ShapePath& path,
    const QPointF& center,
    double maxRadius,
    double strength,
    double twistDegrees)
{
    ShapePath result;
    for (const auto& command : path.commands()) {
        switch (command.type) {
        case PathCommandType::MoveTo:
            result.moveTo(transformRadially(command.points[0], center, maxRadius, strength, twistDegrees));
            break;
        case PathCommandType::LineTo:
            result.lineTo(transformRadially(command.points[0], center, maxRadius, strength, twistDegrees));
            break;
        case PathCommandType::QuadTo:
            result.quadTo(
                transformRadially(command.points[0], center, maxRadius, strength, twistDegrees),
                transformRadially(command.points[1], center, maxRadius, strength, twistDegrees));
            break;
        case PathCommandType::CubicTo:
            result.cubicTo(
                transformRadially(command.points[0], center, maxRadius, strength, twistDegrees),
                transformRadially(command.points[1], center, maxRadius, strength, twistDegrees),
                transformRadially(command.points[2], center, maxRadius, strength, twistDegrees));
            break;
        case PathCommandType::Close:
            result.close();
            break;
        }
    }
    return result;
}

inline Qt::PenJoinStyle toJoinStyle(LineJoin join)
{
    switch (join) {
    case LineJoin::Round: return Qt::RoundJoin;
    case LineJoin::Bevel: return Qt::BevelJoin;
    case LineJoin::Miter:
    default:
        return Qt::MiterJoin;
    }
}

} // namespace detail

class MergePaths : public ShapeOperator {
    W_OBJECT(MergePaths)
public:
    enum Mode {
        Add = 0,
        Subtract = 1,
        Intersect = 2
    };

    explicit MergePaths(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::MergePaths, parent) {}

    int mode() const { return static_cast<int>(mode_); }
    void setMode(int mode)
    {
        const Mode next = static_cast<Mode>(std::clamp(mode, 0, 2));
        if (mode_ != next) {
            mode_ = next;
            emit modeChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<MergePaths>();
        copy->setMode(mode());
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["mode"] = static_cast<int>(mode_);
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("mode")) setMode(obj["mode"].toInt());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        if (inputPaths.empty()) {
            return result;
        }

        QPainterPath combined = inputPaths.front().toPainterPath();
        for (size_t i = 1; i < inputPaths.size(); ++i) {
            const QPainterPath path = inputPaths[i].toPainterPath();
            switch (mode_) {
            case Add:
                combined = combined.united(path);
                break;
            case Subtract:
                combined = combined.subtracted(path);
                break;
            case Intersect:
                combined = combined.intersected(path);
                break;
            }
        }

        combined = combined.simplified();
        if (!combined.isEmpty()) {
            result.push_back(ShapePath::fromPainterPath(combined));
        }
        return result;
    }

    void modeChanged() W_SIGNAL(modeChanged);

private:
    Mode mode_ = Add;
};

class OffsetPaths : public ShapeOperator {
    W_OBJECT(OffsetPaths)
    Q_PROPERTY(float offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(int join READ joinValue WRITE setJoinValue NOTIFY joinChanged)
    Q_PROPERTY(float miterLimit READ miterLimit WRITE setMiterLimit NOTIFY miterLimitChanged)

public:
    explicit OffsetPaths(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::OffsetPaths, parent) {}

    float offset() const { return offset_; }
    void setOffset(float offset)
    {
        if (offset_ != offset) {
            offset_ = offset;
            emit offsetChanged();
        }
    }

    LineJoin join() const { return join_; }
    int joinValue() const { return static_cast<int>(join_); }
    void setJoin(LineJoin join)
    {
        if (join_ != join) {
            join_ = join;
            emit joinChanged();
        }
    }

    void setJoinValue(int join)
    {
        setJoin(static_cast<LineJoin>(std::clamp(join, 0, 2)));
    }

    float miterLimit() const { return miterLimit_; }
    void setMiterLimit(float miterLimit)
    {
        if (miterLimit_ != miterLimit) {
            miterLimit_ = miterLimit;
            emit miterLimitChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<OffsetPaths>();
        copy->setOffset(offset_);
        copy->setJoin(join_);
        copy->setMiterLimit(miterLimit_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["offset"] = (double)offset_;
        obj["join"] = static_cast<int>(join_);
        obj["miterLimit"] = (double)miterLimit_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("offset")) setOffset(obj["offset"].toDouble());
        if (obj.contains("join")) setJoin(static_cast<LineJoin>(obj["join"].toInt()));
        if (obj.contains("miterLimit")) setMiterLimit(obj["miterLimit"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        if (inputPaths.empty() || std::abs(offset_) <= 1e-6f) {
            return inputPaths;
        }

        for (const auto& path : inputPaths) {
            const QPainterPath source = path.toPainterPath();
            if (source.isEmpty()) {
                continue;
            }

            QPainterPathStroker stroker;
            stroker.setWidth(std::abs(offset_) * 2.0);
            stroker.setJoinStyle(detail::toJoinStyle(join_));
            stroker.setMiterLimit(miterLimit_);

            const QPainterPath stroke = stroker.createStroke(source);
            QPainterPath offsetPath = (offset_ > 0.0f)
                ? source.united(stroke)
                : source.subtracted(stroke);
            offsetPath = offsetPath.simplified();

            if (!offsetPath.isEmpty()) {
                result.push_back(ShapePath::fromPainterPath(offsetPath));
            }
        }

        return result;
    }

signals:
    void offsetChanged() W_SIGNAL(offsetChanged);
    void joinChanged() W_SIGNAL(joinChanged);
    void miterLimitChanged() W_SIGNAL(miterLimitChanged);

private:
    float offset_ = 10.0f;
    LineJoin join_ = LineJoin::Miter;
    float miterLimit_ = 4.0f;
};

class PuckerBloat : public ShapeOperator {
    W_OBJECT(PuckerBloat)
    Q_PROPERTY(float amount READ amount WRITE setAmount NOTIFY amountChanged)

public:
    explicit PuckerBloat(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::PuckerBloat, parent) {}

    float amount() const { return amount_; }
    void setAmount(float amount)
    {
        if (amount_ != amount) {
            amount_ = amount;
            emit amountChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<PuckerBloat>();
        copy->setAmount(amount_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["amount"] = (double)amount_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("amount")) setAmount(obj["amount"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        for (const auto& path : inputPaths) {
            const QPointF center = path.boundingRect().center();
            const double maxRadius = detail::maxRadiusFromCenter(path, center);
            const double strength = std::clamp(static_cast<double>(amount_) / 100.0, -1.0, 1.0);
            result.push_back(detail::warpPath(path, center, maxRadius, strength, 0.0));
        }

        return result;
    }

signals:
    void amountChanged() W_SIGNAL(amountChanged);

private:
    float amount_ = 0.0f;
};

class Twist : public ShapeOperator {
    W_OBJECT(Twist)
    Q_PROPERTY(float angle READ angle WRITE setAngle NOTIFY angleChanged)

public:
    explicit Twist(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::Twist, parent) {}

    float angle() const { return angle_; }
    void setAngle(float angle)
    {
        if (angle_ != angle) {
            angle_ = angle;
            emit angleChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<Twist>();
        copy->setAngle(angle_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["angle"] = (double)angle_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("angle")) setAngle(obj["angle"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        for (const auto& path : inputPaths) {
            const QPointF center = path.boundingRect().center();
            const double maxRadius = detail::maxRadiusFromCenter(path, center);
            result.push_back(detail::warpPath(path, center, maxRadius, 0.0, static_cast<double>(angle_)));
        }

        return result;
    }

signals:
    void angleChanged() W_SIGNAL(angleChanged);

private:
    float angle_ = 0.0f;
};

class RoundedCorners : public ShapeOperator {
    W_OBJECT(RoundedCorners)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    explicit RoundedCorners(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::RoundedCorners, parent) {}

    float radius() const { return radius_; }
    void setRadius(float radius)
    {
        if (radius_ != radius) {
            radius_ = radius;
            emit radiusChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<RoundedCorners>();
        copy->setRadius(radius_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["radius"] = (double)radius_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("radius")) setRadius(obj["radius"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        for (const auto& path : inputPaths) {
            QPainterPath rounded;
            const auto segments = path.toSegments();
            if (segments.empty()) {
                result.push_back(path);
                continue;
            }

            const double r = std::max(0.0f, radius_);
            const double clampRadius = std::min(r, 100000.0);

            rounded.moveTo(segments.front().p0);
            for (const auto& segment : segments) {
                const QPointF start = segment.p0;
                const QPointF end = segment.p1;
                const QPointF inVec = segment.cp1 - start;
                const QPointF outVec = segment.cp2 - end;

                const double inLen = detail::length(inVec);
                const double outLen = detail::length(outVec);
                const double localRadius = std::min({clampRadius, inLen / 2.0, outLen / 2.0});

                if (localRadius <= 1e-6) {
                    rounded.lineTo(end);
                    continue;
                }

                const QPointF inDir = inLen > 1e-6 ? inVec / inLen : QPointF();
                const QPointF outDir = outLen > 1e-6 ? outVec / outLen : QPointF();
                const QPointF p0 = start + inDir * localRadius;
                const QPointF p3 = end - outDir * localRadius;

                if (segment.p0 == segment.cp1 && segment.cp2 == segment.p1) {
                    rounded.lineTo(p0);
                    rounded.cubicTo(p0, p3, p3);
                } else {
                    rounded.lineTo(p0);
                    const QPointF c1 = p0 + inDir * (localRadius * 0.5522847498);
                    const QPointF c2 = p3 - outDir * (localRadius * 0.5522847498);
                    rounded.cubicTo(c1, c2, p3);
                }
            }

            if (path.isClosed()) {
                rounded.closeSubpath();
            }
            result.push_back(ShapePath::fromPainterPath(rounded.simplified()));
        }

        return result;
    }

signals:
    void radiusChanged() W_SIGNAL(radiusChanged);

private:
    float radius_ = 8.0f;
};

class WigglePaths : public ShapeOperator {
    W_OBJECT(WigglePaths)
    Q_PROPERTY(float amount READ amount WRITE setAmount NOTIFY amountChanged)
    Q_PROPERTY(float frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)

public:
    explicit WigglePaths(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::WigglePaths, parent) {}

    float amount() const { return amount_; }
    void setAmount(float amount)
    {
        if (amount_ != amount) {
            amount_ = amount;
            emit amountChanged();
        }
    }

    float frequency() const { return frequency_; }
    void setFrequency(float frequency)
    {
        if (frequency_ != frequency) {
            frequency_ = frequency;
            emit frequencyChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<WigglePaths>();
        copy->setAmount(amount_);
        copy->setFrequency(frequency_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["amount"] = (double)amount_;
        obj["frequency"] = (double)frequency_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("amount")) setAmount(obj["amount"].toDouble());
        if (obj.contains("frequency")) setFrequency(obj["frequency"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        for (const auto& path : inputPaths) {
            const QPointF center = path.boundingRect().center();
            const double maxRadius = detail::maxRadiusFromCenter(path, center);
            const double amplitude = std::max(0.0f, amount_);
            const double freq = std::max(0.0f, frequency_);
            ShapePath warped;
            const auto segments = path.toSegments();
            if (segments.empty()) {
                result.push_back(path);
                continue;
            }

            warped.moveTo(segments.front().p0);
            for (const auto& segment : segments) {
                auto wigglePoint = [&](const QPointF& p, double phase) {
                    QPointF radial = p - center;
                    QPointF tangent(-radial.y(), radial.x());
                    const double len = detail::length(tangent);
                    if (len <= 1e-6) {
                        return p;
                    }
                    tangent = tangent / len;
                    const double influence = maxRadius > 1e-6 ? std::clamp(detail::distance(p, center) / maxRadius, 0.0, 1.0) : 1.0;
                    const double offset = std::sin(phase + p.x() * 0.013 + p.y() * 0.017) * amplitude * influence;
                    return p + tangent * offset;
                };

                warped.lineTo(wigglePoint(segment.p1, freq));
            }
            if (path.isClosed()) {
                warped.close();
            }
            result.push_back(warped);
        }

        return result;
    }

signals:
    void amountChanged() W_SIGNAL(amountChanged);
    void frequencyChanged() W_SIGNAL(frequencyChanged);

private:
    float amount_ = 8.0f;
    float frequency_ = 1.0f;
};

class ZigZag : public ShapeOperator {
    W_OBJECT(ZigZag)
    Q_PROPERTY(float amount READ amount WRITE setAmount NOTIFY amountChanged)
    Q_PROPERTY(float frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)

public:
    explicit ZigZag(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::ZigZag, parent) {}

    float amount() const { return amount_; }
    void setAmount(float amount)
    {
        if (amount_ != amount) {
            amount_ = amount;
            emit amountChanged();
        }
    }

    float frequency() const { return frequency_; }
    void setFrequency(float frequency)
    {
        if (frequency_ != frequency) {
            frequency_ = frequency;
            emit frequencyChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<ZigZag>();
        copy->setAmount(amount_);
        copy->setFrequency(frequency_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["amount"] = (double)amount_;
        obj["frequency"] = (double)frequency_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("amount")) setAmount(obj["amount"].toDouble());
        if (obj.contains("frequency")) setFrequency(obj["frequency"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        for (const auto& path : inputPaths) {
            const auto segments = path.toSegments();
            if (segments.empty()) {
                result.push_back(path);
                continue;
            }

            ShapePath warped;
            warped.moveTo(segments.front().p0);
            const double freq = std::max(0.0f, frequency_);
            const double amplitude = std::max(0.0f, amount_);
            for (size_t i = 0; i < segments.size(); ++i) {
                const auto& segment = segments[i];
                const QPointF dir = segment.p1 - segment.p0;
                const double len = detail::length(dir);
                if (len <= 1e-6) {
                    warped.lineTo(segment.p1);
                    continue;
                }
                const QPointF normal(-dir.y() / len, dir.x() / len);
                const double phase = static_cast<double>(i) * freq;
                const QPointF mid = segment.p0 + dir * 0.5;
                const QPointF peak = mid + normal * std::sin(phase) * amplitude;
                warped.lineTo(peak);
                warped.lineTo(segment.p1);
            }
            if (path.isClosed()) {
                warped.close();
            }
            result.push_back(warped);
        }

        return result;
    }

signals:
    void amountChanged() W_SIGNAL(amountChanged);
    void frequencyChanged() W_SIGNAL(frequencyChanged);

private:
    float amount_ = 8.0f;
    float frequency_ = 1.0f;
};

class HandDrawnWobble : public ShapeOperator {
    W_OBJECT(HandDrawnWobble)
    Q_PROPERTY(float wobbleAmount READ wobbleAmount WRITE setWobbleAmount NOTIFY wobbleAmountChanged)
    Q_PROPERTY(float wobbleFrequency READ wobbleFrequency WRITE setWobbleFrequency NOTIFY wobbleFrequencyChanged)
    Q_PROPERTY(float pressureJitter READ pressureJitter WRITE setPressureJitter NOTIFY pressureJitterChanged)
    Q_PROPERTY(float gapProbability READ gapProbability WRITE setGapProbability NOTIFY gapProbabilityChanged)

public:
    explicit HandDrawnWobble(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::HandDrawnWobble, parent) {}

    float wobbleAmount() const { return wobbleAmount_; }
    void setWobbleAmount(float v)
    {
        if (wobbleAmount_ != v) { wobbleAmount_ = v; emit wobbleAmountChanged(); }
    }

    float wobbleFrequency() const { return wobbleFrequency_; }
    void setWobbleFrequency(float v)
    {
        if (wobbleFrequency_ != v) { wobbleFrequency_ = v; emit wobbleFrequencyChanged(); }
    }

    float pressureJitter() const { return pressureJitter_; }
    void setPressureJitter(float v)
    {
        if (pressureJitter_ != v) { pressureJitter_ = v; emit pressureJitterChanged(); }
    }

    float gapProbability() const { return gapProbability_; }
    void setGapProbability(float v)
    {
        if (gapProbability_ != v) { gapProbability_ = v; emit gapProbabilityChanged(); }
    }

    std::unique_ptr<ShapeOperator> clone() const override
    {
        auto copy = std::make_unique<HandDrawnWobble>();
        copy->setWobbleAmount(wobbleAmount_);
        copy->setWobbleFrequency(wobbleFrequency_);
        copy->setPressureJitter(pressureJitter_);
        copy->setGapProbability(gapProbability_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["wobbleAmount"] = (double)wobbleAmount_;
        obj["wobbleFrequency"] = (double)wobbleFrequency_;
        obj["pressureJitter"] = (double)pressureJitter_;
        obj["gapProbability"] = (double)gapProbability_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("wobbleAmount")) setWobbleAmount(obj["wobbleAmount"].toDouble());
        if (obj.contains("wobbleFrequency")) setWobbleFrequency(obj["wobbleFrequency"].toDouble());
        if (obj.contains("pressureJitter")) setPressureJitter(obj["pressureJitter"].toDouble());
        if (obj.contains("gapProbability")) setGapProbability(obj["gapProbability"].toDouble());
    }

    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override
    {
        std::vector<ShapePath> result;
        result.reserve(inputPaths.size());

        // 簡易ハッシュベースの疑似乱数（シード固定で再現性確保）
        auto hashNoise = [](double x, double y, double seed) -> double {
            uint64_t h = std::bit_cast<uint64_t>(x) * 374761393ULL +
                         std::bit_cast<uint64_t>(y) * 668265263ULL +
                         std::bit_cast<uint64_t>(seed) * 1274126177ULL;
            h = (h ^ (h >> 27)) * 0x3C79AC492BA7B653ULL;
            h = (h ^ (h >> 31));
            return static_cast<double>(h & 0x7FFFFFFF) / 2147483648.0;
        };

        // Perlin-like スムーズノイズ（xxHash ベース、方向偏り軽減）
        auto smoothNoise = [&](double x, double y, double seed) -> double {
            const double fx = std::floor(x);
            const double fy = std::floor(y);
            const double fracX = x - fx;
            const double fracY = y - fy;
            const double sx = fracX * fracX * (3.0 - 2.0 * fracX);
            const double sy = fracY * fracY * (3.0 - 2.0 * fracY);

            const double n00 = hashNoise(fx + 0.0, fy + 0.0, seed);
            const double n10 = hashNoise(fx + 1.0, fy + 0.0, seed);
            const double n01 = hashNoise(fx + 0.0, fy + 1.0, seed);
            const double n11 = hashNoise(fx + 1.0, fy + 1.0, seed);

            const double nx0 = n00 + (n10 - n00) * sx;
            const double nx1 = n01 + (n11 - n01) * sx;
            return nx0 + (nx1 - nx0) * sy;
        };

        for (const auto& path : inputPaths) {
            const auto segments = path.toSegments();
            if (segments.empty()) {
                result.push_back(path);
                continue;
            }

            const double amp = std::max(0.0f, wobbleAmount_);
            const double freq = std::max(0.1f, wobbleFrequency_);
            const double jit = std::clamp(0.0, static_cast<double>(pressureJitter_), 1.0);
            const double gapProb = std::clamp(0.0, static_cast<double>(gapProbability_), 0.95);

            ShapePath warped;
            bool gapActive = false;

            for (size_t i = 0; i < segments.size(); ++i) {
                const auto& seg = segments[i];

                // gapProbability に基づいてセグメントを間引く（かすれ表現）
                if (gapProb > 0.0 && i > 0) {
                    const double gapRoll = hashNoise(seg.p1.x(), seg.p1.y(), 999.0 + static_cast<double>(i));
                    gapActive = gapRoll < gapProb;
                }

                if (gapActive) {
                    // かすれ区間：スキップ（MoveTo は維持）
                    warped.lineTo(seg.p1);
                    continue;
                }

                // 筆圧風ジッター：振幅にランダム変調を掛ける
                const double pressureMod = 1.0 - jit * smoothNoise(seg.p1.x() * 0.1, seg.p1.y() * 0.1, 42.0);
                const double effectiveAmp = amp * (0.3 + 0.7 * pressureMod);

                auto wobblePoint = [&](const QPointF& p, double phase) -> QPointF {
                    const double noiseX = smoothNoise(p.x() * freq * 0.01, p.y() * freq * 0.01, phase);
                    const double noiseY = smoothNoise(p.x() * freq * 0.01 + 1000.0, p.y() * freq * 0.01 + 1000.0, phase + 500.0);
                    const double angle = noiseY * 2.0 * std::numbers::pi;
                    const double offset = (noiseX - 0.5) * 2.0 * effectiveAmp;
                    return QPointF(p.x() + std::cos(angle) * offset, p.y() + std::sin(angle) * offset);
                };

                // 制御点も揺らす
                if (seg.p0 == seg.cp1 && seg.cp2 == seg.p1) {
                    // 直線セグメント
                    if (i == 0) warped.moveTo(wobblePoint(seg.p0, 0.0));
                    warped.lineTo(wobblePoint(seg.p1, static_cast<double>(i)));
                } else {
                    // 曲線セグメント
                    if (i == 0) warped.moveTo(wobblePoint(seg.p0, 0.0));
                    const double phase = static_cast<double>(i);
                    warped.cubicTo(
                        wobblePoint(seg.cp1, phase + 0.1),
                        wobblePoint(seg.cp2, phase + 0.2),
                        wobblePoint(seg.p1, phase + 0.3));
                }
            }

            if (path.isClosed()) {
                warped.close();
            }
            result.push_back(warped);
        }

        return result;
    }

signals:
    void wobbleAmountChanged() W_SIGNAL(wobbleAmountChanged);
    void wobbleFrequencyChanged() W_SIGNAL(wobbleFrequencyChanged);
    void pressureJitterChanged() W_SIGNAL(pressureJitterChanged);
    void gapProbabilityChanged() W_SIGNAL(gapProbabilityChanged);

private:
    float wobbleAmount_ = 4.0f;
    float wobbleFrequency_ = 2.0f;
    float pressureJitter_ = 0.4f;
    float gapProbability_ = 0.0f;
};

W_OBJECT_IMPL(MergePaths)
W_OBJECT_IMPL(OffsetPaths)
W_OBJECT_IMPL(PuckerBloat)
W_OBJECT_IMPL(Twist)
W_OBJECT_IMPL(RoundedCorners)
W_OBJECT_IMPL(WigglePaths)
W_OBJECT_IMPL(ZigZag)
W_OBJECT_IMPL(HandDrawnWobble)

} // namespace ArtifactCore
