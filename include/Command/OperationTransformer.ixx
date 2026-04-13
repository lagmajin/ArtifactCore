module;
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

export module Command.OperationTransformer;

export namespace ArtifactCore {

/**
 * @brief OT (Operational Transformation) による競合解決
 *
 * 複数のクライアントが同時に操作を行った際、後発の操作を
 * 先行操作に対して変換（transform）して整合性を保つ。
 */
class OperationTransformer {
public:
    /**
     * @brief 2つの操作を競合解決
     * @param op 後発操作
     * @param againstOp 先行操作
     * @return 変換された操作
     */
    static QJsonObject transform(const QJsonObject& op, const QJsonObject& againstOp) {
        QString targetType = getTargetPath(op);
        QString againstTarget = getTargetPath(againstOp);

        // 同一ターゲットでない場合は変換不要
        if (targetType != againstTarget) {
            return op;
        }

        QString opType = op.value(QStringLiteral("type")).toString();
        QString againstType = againstOp.value(QStringLiteral("type")).toString();

        // 両方とも setProperty の場合
        if (opType == QStringLiteral("setProperty") && againstType == QStringLiteral("setProperty")) {
            return transformSetProperty(op, againstOp);
        }

        // レイヤー操作の場合
        if (opType.startsWith(QStringLiteral("layer.")) && againstType.startsWith(QStringLiteral("layer."))) {
            return transformLayerOperation(op, againstOp);
        }

        // デフォルト: 後勝ち（opをそのまま返す）
        return op;
    }

    /**
     * @brief 2つの操作が互換性があるか（並行実行可能か）
     */
    static bool areCompatible(const QJsonObject& a, const QJsonObject& b) {
        QString targetA = getTargetPath(a);
        QString targetB = getTargetPath(b);
        return targetA == targetB;
    }

    /**
     * @brief 操作のターゲットパスを抽出
     */
    static QString getTargetPath(const QJsonObject& op) {
        return op.value(QStringLiteral("target")).toString();
    }

private:
    /**
     * @brief setProperty操作の競合解決
     *
     * 同一プロパティへの同時更新: 後発の値で上書き
     * 異なるプロパティ: 両方適用（変換不要）
     */
    static QJsonObject transformSetProperty(const QJsonObject& op, const QJsonObject& againstOp) {
        QString opProp = op.value(QStringLiteral("property")).toString();
        QString againstProp = againstOp.value(QStringLiteral("property")).toString();

        // 同一プロパティの場合、後発の操作が優先（そのまま返す）
        // これは「最後の書き込みが勝つ」セマンティクス
        QJsonObject result = op;
        result[QStringLiteral("transformed")] = (opProp == againstProp);
        return result;
    }

    /**
     * @brief レイヤー操作の競合解決
     *
     * レイヤー追加 vs レイヤー削除: 削除が勝つ
     * レイヤー移動 vs レイヤー削除: 移動は無視
     */
    static QJsonObject transformLayerOperation(const QJsonObject& op, const QJsonObject& againstOp) {
        QString opType = op.value(QStringLiteral("type")).toString();
        QString againstType = againstOp.value(QStringLiteral("type")).toString();

        // レイヤー削除が先行した場合、他の操作は無効化
        if (againstType == QStringLiteral("layer.remove")) {
            QJsonObject noop;
            noop[QStringLiteral("type")] = QStringLiteral("noop");
            noop[QStringLiteral("reason")] = QStringLiteral("Target layer was removed");
            return noop;
        }

        return op;
    }
};

} // namespace ArtifactCore
