module;
#include <QVector3D>
#include <QMatrix4x4>
export module Core.Mask.DepthMask;

export namespace ArtifactCore {

/// 深度フォールオフカーブ
enum class DepthFalloffCurve {
    Hard,     // 中間でステップ
    Linear,   // 線形
    Smooth    // Smoothstep
};

/// 深度マスクの設定
struct DepthMaskSettings {
    float nearDistance = 0.0f;      ///< カメラからのZ距離（マスク開始）
    float farDistance = 1000.0f;    ///< カメラからのZ距離（マスク完了）
    DepthFalloffCurve falloff = DepthFalloffCurve::Smooth;
    bool invert = false;
};

/// カメラのview/projection行列とレイヤー位置から深度マスクα値を計算する
class DepthMaskCalculator {
public:
    /// レイヤーのワールド位置→カメラZ→α値を一括計算
    static float computeAlpha(
        const QVector3D& layerPosition,
        const QMatrix4x4& viewMatrix,
        const QMatrix4x4& projectionMatrix,
        const DepthMaskSettings& settings
    );
};

}
