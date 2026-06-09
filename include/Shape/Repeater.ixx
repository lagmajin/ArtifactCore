module;

#include <vector>
#include <cmath>
#include <QObject>
#include <QPointF>
#include <QTransform>
#include <wobjectdefs.h>
#include <wobjectimpl.h>

export module Shape.Repeater;

import Shape.Operator;
import Shape.Path;

export namespace ArtifactCore {

/**
 * @brief リピーター演算子
 * 
 * 入力されたパスを複数回複製し、それぞれに累積トランスフォームを適用する。
 * AEのシェイプレイヤーにある「リピーター」と同等の機能。
 */
class Repeater : public ShapeOperator {
    W_OBJECT(Repeater)
    Q_PROPERTY(int copies READ copies WRITE setCopies NOTIFY copiesChanged)
    Q_PROPERTY(float offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(QPointF anchorPoint READ anchorPoint WRITE setAnchorPoint NOTIFY anchorPointChanged)
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QPointF scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(float rotation READ rotation WRITE setRotation NOTIFY rotationChanged)

public:
    explicit Repeater(QObject* parent = nullptr)
        : ShapeOperator(ShapeOperatorType::Repeater, parent) {}

    int copies() const { return copies_; }
    void setCopies(int c) {
        if (copies_ != c) {
            copies_ = c;
            emit copiesChanged();
        }
    }

    float offset() const { return offset_; }
    void setOffset(float o) {
        if (offset_ != o) {
            offset_ = o;
            emit offsetChanged();
        }
    }

    QPointF anchorPoint() const { return anchorPoint_; }
    void setAnchorPoint(const QPointF& ap) {
        if (anchorPoint_ != ap) {
            anchorPoint_ = ap;
            emit anchorPointChanged();
        }
    }

    QPointF position() const { return position_; }
    void setPosition(const QPointF& p) {
        if (position_ != p) {
            position_ = p;
            emit positionChanged();
        }
    }

    QPointF scale() const { return scale_; }
    void setScale(const QPointF& s) {
        if (scale_ != s) {
            scale_ = s;
            emit scaleChanged();
        }
    }

    float rotation() const { return rotation_; }
    void setRotation(float r) {
        if (rotation_ != r) {
            rotation_ = r;
            emit rotationChanged();
        }
    }

    std::unique_ptr<ShapeOperator> clone() const override {
        auto copy = std::make_unique<Repeater>();
        copy->setCopies(copies_);
        copy->setOffset(offset_);
        copy->setAnchorPoint(anchorPoint_);
        copy->setPosition(position_);
        copy->setScale(scale_);
        copy->setRotation(rotation_);
        return copy;
    }

    /**
     * @brief パスを複製して累積変形を適用する
     */
    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override {
        std::vector<ShapePath> result;
        if (copies_ <= 0 || inputPaths.empty()) return inputPaths;

        result.reserve(inputPaths.size() * copies_);

        for (int i = 0; i < copies_; ++i) {
            double index = static_cast<double>(i) + offset_;

            QTransform matrix;
            // AE standard repeater accumulation:
            // 1. Translate to anchorPoint of the repeater
            // 2. Translate by position * index, rotate by rotation * index, scale by scale^index
            // 3. Translate back by anchorPoint
            matrix.translate(anchorPoint_.x(), anchorPoint_.y());
            matrix.translate(position_.x() * index, position_.y() * index);
            matrix.rotate(rotation_ * index);
            matrix.scale(std::pow(scale_.x(), index), std::pow(scale_.y(), index));
            matrix.translate(-anchorPoint_.x(), -anchorPoint_.y());

            for (const auto& path : inputPaths) {
                ShapePath copyPath = path.clone();
                copyPath.transform(matrix);
                result.push_back(copyPath);
            }
        }
        return result;
    }

    void copiesChanged() W_SIGNAL(copiesChanged);
    void offsetChanged() W_SIGNAL(offsetChanged);
    void anchorPointChanged() W_SIGNAL(anchorPointChanged);
    void positionChanged() W_SIGNAL(positionChanged);
    void scaleChanged() W_SIGNAL(scaleChanged);
    void rotationChanged() W_SIGNAL(rotationChanged);

private:
    int copies_ = 3;
    float offset_ = 0.0f;
    QPointF anchorPoint_ = QPointF(0.0f, 0.0f);
    QPointF position_ = QPointF(100.0f, 0.0f); // Default translation per copy
    QPointF scale_ = QPointF(1.0f, 1.0f);       // 100% scale
    float rotation_ = 0.0f;
};

W_OBJECT_IMPL(Repeater)

} // namespace ArtifactCore
