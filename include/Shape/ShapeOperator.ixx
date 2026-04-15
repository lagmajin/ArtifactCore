module;

#include <vector>
#include <memory>
#include <QObject>

export module Shape.Operator;

import Shape.Path;

export namespace ArtifactCore {

/**
 * @brief シェイプ演算子の種類
 */
enum class ShapeOperatorType {
    None,
    TrimPaths,      // トリムパス
    Repeater,       // 繰り返
    OffsetPaths,    // パスのオフセット
    WigglePaths,    // パスのゆらぎ
    ZigZag,         // ぎざぎざ
    PuckerBloat,    // 膨張・収縮
    RoundedCorners, // 角丸
    Twist           // ねじれ
};

/**
 * @brief シェイプ演算子の基底クラス
 * 
 * AEの「パスをグループ化」内の「パスを追加」→「トリムパス」などの機能に相当。
 * ShapeGroup 内のパスリストに対して処理を行い、新しいパスリストを返す。
 */
class ShapeOperator : public QObject {
    Q_OBJECT
public:
    explicit ShapeOperator(ShapeOperatorType type, QObject* parent = nullptr) : QObject(parent), type_(type) {}
    virtual ~ShapeOperator() = default;

    ShapeOperatorType type() const { return type_; }

    /**
     * @brief パスに演算子を適用する
     * @param inputPaths 入力パスのリスト
     * @return 変換後のパスのリスト
     */
    virtual std::vector<ShapePath> process(const std::vector<ShapePath>& inputPaths) const = 0;

private:
    ShapeOperatorType type_;
};

} // namespace ArtifactCore
