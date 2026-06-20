module;

#include <vector>
#include <cmath>
#include <QObject>
#include <QPointF>
#include <QTransform>

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

    float startOpacity() const { return startOpacity_; }
    void setStartOpacity(float o) {
        if (startOpacity_ != o) {
            startOpacity_ = o;
            emit startOpacityChanged();
        }
    }

    float endOpacity() const { return endOpacity_; }
    void setEndOpacity(float o) {
        if (endOpacity_ != o) {
            endOpacity_ = o;
            emit endOpacityChanged();
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
        copy->setStartOpacity(startOpacity_);
        copy->setEndOpacity(endOpacity_);
        return copy;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["copies"] = copies_;
        obj["offset"] = (double)offset_;
        obj["anchorX"] = anchorPoint_.x();
        obj["anchorY"] = anchorPoint_.y();
        obj["posX"] = position_.x();
        obj["posY"] = position_.y();
        obj["scaleX"] = scale_.x();
        obj["scaleY"] = scale_.y();
        obj["rotation"] = (double)rotation_;
        obj["startOpacity"] = (double)startOpacity_;
        obj["endOpacity"] = (double)endOpacity_;
        return obj;
    }

    void fromJson(const QJsonObject& obj) override {
        if (obj.contains("copies")) setCopies(obj["copies"].toInt());
        if (obj.contains("offset")) setOffset(obj["offset"].toDouble());
        if (obj.contains("anchorX")) setAnchorPoint(QPointF(obj["anchorX"].toDouble(), obj["anchorY"].toDouble()));
        if (obj.contains("posX")) setPosition(QPointF(obj["posX"].toDouble(), obj["posY"].toDouble()));
        if (obj.contains("scaleX")) setScale(QPointF(obj["scaleX"].toDouble(), obj["scaleY"].toDouble()));
        if (obj.contains("rotation")) setRotation(obj["rotation"].toDouble());
        if (obj.contains("startOpacity")) setStartOpacity(obj["startOpacity"].toDouble());
        if (obj.contains("endOpacity")) setEndOpacity(obj["endOpacity"].toDouble());
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

            double t = (copies_ > 1) ? (static_cast<double>(i) / (copies_ - 1)) : 0.0;
            double opacityMult = startOpacity_ + (endOpacity_ - startOpacity_) * t;

            for (const auto& path : inputPaths) {
                ShapePath copyPath = path.clone();
                copyPath.transform(matrix);
                copyPath.setOpacity(path.opacity() * opacityMult);
                result.push_back(copyPath);
            }
        }
        return result;
    }

    void copiesChanged() {}
    void offsetChanged() {}
    void anchorPointChanged() {}
    void positionChanged() {}
    void scaleChanged() {}
    void rotationChanged() {}
    void startOpacityChanged() {}
    void endOpacityChanged() {}

private:
    int copies_ = 3;
    float offset_ = 0.0f;
    QPointF anchorPoint_ = QPointF(0.0f, 0.0f);
    QPointF position_ = QPointF(100.0f, 0.0f); // Default translation per copy
    QPointF scale_ = QPointF(1.0f, 1.0f);       // 100% scale
    float rotation_ = 0.0f;
    float startOpacity_ = 1.0f;
    float endOpacity_ = 1.0f;
};

} // namespace ArtifactCore
