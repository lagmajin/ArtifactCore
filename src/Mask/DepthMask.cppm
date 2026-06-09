module;
#include <algorithm>
#include <QVector3D>
#include <QMatrix4x4>
module Core.Mask.DepthMask;

namespace ArtifactCore {

float DepthMaskCalculator::computeAlpha(
    const QVector3D& layerPosition,
    const QMatrix4x4& viewMatrix,
    const QMatrix4x4& projectionMatrix,
    const DepthMaskSettings& settings)
{
    Q_UNUSED(projectionMatrix);

    // ワールド位置 → カメラ空間へ変換
    const QVector4D camPos = viewMatrix * QVector4D(layerPosition, 1.0f);
    // OpenGL右手系: カメラ前方は-Z。正のZ距離にする
    const float camZ = -camPos.z();

    const float range = settings.farDistance - settings.nearDistance;
    if (range <= 0.0f)
        return settings.invert ? 1.0f : 0.0f;

    float t = (camZ - settings.nearDistance) / range;
    t = std::clamp(t, 0.0f, 1.0f);

    float alpha;
    switch (settings.falloff) {
        case DepthFalloffCurve::Hard:
            alpha = t >= 0.5f ? 1.0f : 0.0f;
            break;
        case DepthFalloffCurve::Linear:
            alpha = t;
            break;
        case DepthFalloffCurve::Smooth:
            alpha = t * t * (3.0f - 2.0f * t);
            break;
    }

    return settings.invert ? 1.0f - alpha : alpha;
}

}
