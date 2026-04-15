module;

#include <vector>
#include <QPainterPath>
#include <QPainterPathStroker>

export module Shape.TrimPaths;

import Shape.Operator;

export namespace ArtifactCore {

/**
 * @brief トリムパス演算子
 * 
 * パスの開始点、終了点、オフセットを指定してパスを切り取る。
 * AEのシェイプレイヤーにある「トリムパス」と同等の機能。
 */
class TrimPaths : public ShapeOperator {
    Q_OBJECT
    Q_PROPERTY(float start READ start WRITE setStart NOTIFY startChanged)
    Q_PROPERTY(float end READ end WRITE setEnd NOTIFY endChanged)
    Q_PROPERTY(float offset READ offset WRITE setOffset NOTIFY offsetChanged)

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

    /**
     * @brief パスをトリムする
     */
    std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const override {
        // TODO: 厳密な Trim Path 幾何学計算の実装
        // 現在 ShapePath クラスには toQPainterPath() がないため、
        // 今後のタスクとして ShapePath に変換メソッドを追加し、
        // ここで QPainterPath に変換 -> 切り取り -> ShapePath に戻す 処理を行う。
        // 暫定的に入力をそのまま返す。
        return inputPaths;
    }

signals:
    void startChanged();
    void endChanged();
    void offsetChanged();

private:
    float start_ = 0.0f;   // 0.0 - 100.0
    float end_ = 100.0f;   // 0.0 - 100.0
    float offset_ = 0.0f;  // 0.0 - 360.0 degrees
};

} // namespace ArtifactCore
