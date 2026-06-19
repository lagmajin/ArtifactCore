module;

#include <vector>
#include <cmath>
#include <algorithm>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPointF>

export module Shape.TrimPaths;

import Container.NamedVector;
import Shape.Operator;
import Shape.Path;
import Shape.Types;

export namespace ArtifactCore {

/**
 * @brief トリムパスのモード
 */
export enum class TrimMode {
    Simultaneously, ///< 同時に（全てのパスを1本の連続したパスとして扱う）
    Individually    ///< 個別に（それぞれのパスを独立して0-100%で扱う）
};

/**
 * @brief トリムパス演算子
 * 
 * パスの開始点、終了点、オフセットを指定してパスを切り取る。
 * AEのシェイプレイヤーにある「トリムパス」と同等の機能。
 */
class TrimPaths : public ShapeOperator {
    Q_PROPERTY(float start READ start WRITE setStart NOTIFY startChanged)
    Q_PROPERTY(float end READ end WRITE setEnd NOTIFY endChanged)
    Q_PROPERTY(float offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(TrimMode trimMode READ trimMode WRITE setTrimMode NOTIFY trimModeChanged)

public:
    explicit TrimPaths(QObject* parent = nullptr) 
        : ShapeOperator(ShapeOperatorType::TrimPaths, parent) {}

    float start() const { return start_; }
    void setStart(float s) { 
        if (start_ != s) {
            start_ = s; 
            emit startChanged(); 
        }
    }

    float end() const { return end_; }
    void setEnd(float e) { 
        if (end_ != e) {
            end_ = e; 
            emit endChanged(); 
        }
    }

    float offset() const { return offset_; }
    void setOffset(float o) { 
        if (offset_ != o) {
            offset_ = o; 
            emit offsetChanged(); 
        }
    }

    TrimMode trimMode() const { return trimMode_; }
    void setTrimMode(TrimMode mode) {
        if (trimMode_ != mode) {
            trimMode_ = mode;
            emit trimModeChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override {
        auto copy = std::make_unique<TrimPaths>();
        copy->setStart(start_);
        copy->setEnd(end_);
        copy->setOffset(offset_);
        copy->setTrimMode(trimMode_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["start"] = (double)start_;
        obj["end"] = (double)end_;
        obj["offset"] = (double)offset_;
        obj["trimMode"] = static_cast<int>(trimMode_);
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("start")) setStart(obj["start"].toDouble());
        if (obj.contains("end")) setEnd(obj["end"].toDouble());
        if (obj.contains("offset")) setOffset(obj["offset"].toDouble());
        if (obj.contains("trimMode")) setTrimMode(static_cast<TrimMode>(obj["trimMode"].toInt()));
    }

    /**
     * @brief パスをトリムする
     */
    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override {
        NamedVector<ShapePath> result{makeNamedVector<ShapePath>(ContainerName{"TrimPathsResult"})};
        if (inputPaths.empty()) return result;

        double s = static_cast<double>(start_) / 100.0;
        double e = static_cast<double>(end_) / 100.0;
        double offsetPercent = static_cast<double>(offset_) / 360.0;

        // Apply offset
        s += offsetPercent;
        e += offsetPercent;

        // Normalize between 0 and 1
        auto normalize = [](double val) {
            double v = std::fmod(val, 1.0);
            if (v < 0.0) v += 1.0;
            return v;
        };

        double t_start = normalize(s);
        double t_end = normalize(e);

        if (std::abs(t_start - t_end) < 0.0001 && std::abs(s - e) > 0.0001) {
            // Trim covers the whole path (e.g. start=0, end=100)
            return inputPaths;
        }

        if (trimMode_ == TrimMode::Individually) {
            for (const auto& path : inputPaths) {
                if (path.isEmpty()) continue;
                processSinglePath(path, t_start, t_end, result);
            }
        } else {
            // Simultaneously: treat all paths as one combined length
            std::vector<double> pathLengths;
            double totalLen = 0.0;
            for (const auto& path : inputPaths) {
                double len = calculatePathLength(path);
                pathLengths.push_back(len);
                totalLen += len;
            }

            if (totalLen <= 0.0001) {
                return inputPaths;
            }

            double targetStart = t_start * totalLen;
            double targetEnd = t_end * totalLen;

            if (targetStart <= targetEnd) {
                processSimultaneousRange(inputPaths, pathLengths, targetStart, targetEnd, result);
            } else {
                // Wrap around
                processSimultaneousRange(inputPaths, pathLengths, targetStart, totalLen, result);
                processSimultaneousRange(inputPaths, pathLengths, 0.0, targetEnd, result);
            }
        }
        return result.toStdVector();
    }

signals:
    void startChanged() {}
    void endChanged() {}
    void offsetChanged() {}
    void trimModeChanged() {}

private:
    float start_ = 0.0f;   // 0.0 - 100.0
    float end_ = 100.0f;   // 0.0 - 100.0
    float offset_ = 0.0f;  // 0.0 - 360.0 degrees
    TrimMode trimMode_ = TrimMode::Individually;

    static double calculatePathLength(const ShapePath& path) {
        if (path.isEmpty()) return 0.0;
        std::vector<BezierSegment> segments = path.toSegments();
        double totalLen = 0.0;
        for (const auto& seg : segments) {
            if (seg.cp1 == seg.p0 && seg.cp2 == seg.p1) {
                totalLen += distance(seg.p0, seg.p1);
            } else {
                totalLen += segmentLength(seg);
            }
        }
        return totalLen;
    }

    void processSinglePath(const ShapePath& path, double t_start, double t_end, NamedVector<ShapePath>& result) const {
        NamedVector<BezierSegment> segments{makeNamedVector<BezierSegment>(ContainerName{"TrimPathsSegments"})};
        for (const auto& seg : path.toSegments()) {
            segments.add(seg);
        }
        if (segments.empty()) return;

        double totalLen = calculatePathLength(path);
        if (totalLen <= 0.0001) {
            result.push_back(path.clone());
            return;
        }

        double targetStart = t_start * totalLen;
        double targetEnd = t_end * totalLen;

        if (targetStart <= targetEnd) {
            auto trimmed = trimSegments(segments.toStdVector(), targetStart, targetEnd);
            if (!trimmed.empty()) {
                ShapePath trimmedPath = segmentsToPath(trimmed);
                trimmedPath.setClosed(false);
                result.add(trimmedPath);
            }
        } else {
            auto trimmed1 = trimSegments(segments.toStdVector(), targetStart, totalLen);
            auto trimmed2 = trimSegments(segments.toStdVector(), 0.0, targetEnd);

            if (!trimmed1.empty()) {
                ShapePath trimmedPath1 = segmentsToPath(trimmed1);
                trimmedPath1.setClosed(false);
                result.add(trimmedPath1);
            }
            if (!trimmed2.empty()) {
                ShapePath trimmedPath2 = segmentsToPath(trimmed2);
                trimmedPath2.setClosed(false);
                result.add(trimmedPath2);
            }
        }
    }

    void processSimultaneousRange(const std::vector<ShapePath>& inputPaths,
                                  const std::vector<double>& pathLengths,
                                  double targetStart, double targetEnd,
                                  NamedVector<ShapePath>& result) const {
        double currentLen = 0.0;
        for (size_t i = 0; i < inputPaths.size(); ++i) {
            double pathLen = pathLengths[i];
            if (pathLen <= 0.0001) continue;

            double pathStart = currentLen;
            double pathEnd = currentLen + pathLen;

            double overlapStart = std::max(pathStart, targetStart);
            double overlapEnd = std::min(pathEnd, targetEnd);

            if (overlapStart < overlapEnd) {
                auto segments = inputPaths[i].toSegments();
                auto trimmed = trimSegments(segments, overlapStart - pathStart, overlapEnd - pathStart);
                if (!trimmed.empty()) {
                    ShapePath trimmedPath = segmentsToPath(trimmed);
                    trimmedPath.setClosed(false);
                    result.add(trimmedPath);
                }
            }
            currentLen = pathEnd;
        }
    }

    static double distance(const QPointF& a, const QPointF& b) {
        double dx = b.x() - a.x();
        double dy = b.y() - a.y();
        return std::sqrt(dx * dx + dy * dy);
    }

    static double segmentLength(const BezierSegment& seg) {
        double len = 0.0;
        QPointF prev = seg.p0;
        for (int i = 1; i <= 10; ++i) {
            QPointF curr = seg.pointAt(static_cast<double>(i) / 10.0);
            len += distance(prev, curr);
            prev = curr;
        }
        return len;
    }

    static BezierSegment splitLeft(const BezierSegment& seg, double t) {
        QPointF p01 = (1.0 - t) * seg.p0 + t * seg.cp1;
        QPointF p12 = (1.0 - t) * seg.cp1 + t * seg.cp2;
        QPointF p23 = (1.0 - t) * seg.cp2 + t * seg.p1;
        QPointF p012 = (1.0 - t) * p01 + t * p12;
        QPointF p123 = (1.0 - t) * p12 + t * p23;
        QPointF p0123 = (1.0 - t) * p012 + t * p123;
        return BezierSegment(seg.p0, p01, p012, p0123);
    }

    static BezierSegment splitRight(const BezierSegment& seg, double t) {
        QPointF p01 = (1.0 - t) * seg.p0 + t * seg.cp1;
        QPointF p12 = (1.0 - t) * seg.cp1 + t * seg.cp2;
        QPointF p23 = (1.0 - t) * seg.cp2 + t * seg.p1;
        QPointF p012 = (1.0 - t) * p01 + t * p12;
        QPointF p123 = (1.0 - t) * p12 + t * p23;
        QPointF p0123 = (1.0 - t) * p012 + t * p123;
        return BezierSegment(p0123, p123, p23, seg.p1);
    }

    static BezierSegment sliceSegment(const BezierSegment& seg, double t0, double t1) {
        if (seg.cp1 == seg.p0 && seg.cp2 == seg.p1) {
            QPointF newP0 = seg.p0 + t0 * (seg.p1 - seg.p0);
            QPointF newP1 = seg.p0 + t1 * (seg.p1 - seg.p0);
            return BezierSegment(newP0, newP0, newP1, newP1);
        } else {
            BezierSegment leftSeg = seg;
            if (t1 < 1.0) {
                leftSeg = splitLeft(seg, t1);
            }
            BezierSegment trimmed = leftSeg;
            if (t0 > 0.0) {
                double t_new = t1 > 0.0 ? t0 / t1 : 0.0;
                trimmed = splitRight(leftSeg, t_new);
            }
            return trimmed;
        }
    }

    static std::vector<BezierSegment> trimSegments(const std::vector<BezierSegment>& segments, double targetStart, double targetEnd) {
        NamedVector<BezierSegment> result{makeNamedVector<BezierSegment>(ContainerName{"TrimPathsTrimmedSegments"})};
        if (targetStart >= targetEnd) return result;

        double currentLen = 0.0;
        for (const auto& seg : segments) {
            double segLen = 0.0;
            if (seg.cp1 == seg.p0 && seg.cp2 == seg.p1) {
                segLen = distance(seg.p0, seg.p1);
            } else {
                segLen = segmentLength(seg);
            }

            if (segLen <= 0.0001) {
                continue;
            }
            double segStart = currentLen;
            double segEnd = currentLen + segLen;

            double overlapStart = std::max(segStart, targetStart);
            double overlapEnd = std::min(segEnd, targetEnd);

            if (overlapStart < overlapEnd) {
                double t0 = (overlapStart - segStart) / segLen;
                double t1 = (overlapEnd - segStart) / segLen;
                result.add(sliceSegment(seg, t0, t1));
            }
            currentLen = segEnd;
        }
        return result.toStdVector();
    }

    static ShapePath segmentsToPath(const std::vector<BezierSegment>& segments) {
        ShapePath path;
        if (segments.empty()) return path;

        path.moveTo(segments[0].p0);
        for (const auto& seg : segments) {
            if (seg.cp1 == seg.p0 && seg.cp2 == seg.p1) {
                path.lineTo(seg.p1);
            } else {
                path.cubicTo(seg.cp1, seg.cp2, seg.p1);
            }
        }
        return path;
    }
};

} // namespace ArtifactCore
