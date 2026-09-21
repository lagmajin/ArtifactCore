module;

#include <QVector3D>
#include <cstdint>

export module Geometry.MeshDeform;

import Mesh;

export namespace ArtifactCore {

// axis: 0=X, 1=Y, 2=Z.

/// 円筒ベンド。axis 方向の座標 t に対し、(axis, (axis+1)%3) 平面内で
/// 曲率 angle [rad/単位長] の円弧へ巻く。angle=0 は恒等変換。
struct BendParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    int axis = 0;
    float angle = 0.0f;
};

/// ねじり。axis 方向の座標 t に比例した角度 angle*t [rad] で
/// 残り2軸を回転する。
struct TwistParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    int axis = 1;
    float angle = 0.0f;
};

/// テーパー。axis 方向の座標 t に対し、残り2軸を (1 + t*amount) 倍する
/// (0 でクランプ)。amount=0 は恒等変換。
struct TaperParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    int axis = 1;
    float amount = 0.0f;
};

/// 法線方向変位。fBm ノイズ(field.fractal, octaves) * amount を法線方向へ加算する。
/// subdivLevels > 0 の場合は先に Catmull-Clark 細分化する。
struct DisplaceParams {
    float amount = 1.0f;
    float frequency = 0.25f;
    int octaves = 3;
    unsigned int seed = 42u;
    int subdivLevels = 0;
};

/// 正弦波。dot(p-center, direction)*frequency + timeSeconds*speed の位相で
/// 法線方向へ amplitude だけ振動させる。
struct WaveParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    QVector3D direction{0.0f, 1.0f, 0.0f};
    float frequency = 0.5f;
    float amplitude = 1.0f;
    float speed = 1.0f;
    float timeSeconds = 0.0f;
};

/// バルジ。center からの距離に応じたガウスフォールオフで放射方向へ
/// 拡縮する (amount>0 で膨張)。radius<=0 で全体に一様適用。
struct BulgeParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    float amount = 0.0f;
};

/// 球面化。頂点を center 中心・半径 radius の球面へ amount (0..1) だけ
/// ブレンドする。
struct SpherifyParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    float amount = 0.0f;
};

/// せん断。p[axis] += (p[shearAxis]-center)*amount。
struct ShearParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    int axis = 0;
    int shearAxis = 1;
    float amount = 0.0f;
};

/// パッカー/AE Pucker&Bloat 相当。放射方向へ (1-amount*falloff) 倍する。
/// amount>0 で吸引、<0 で膨張。radius<=0 で全体に一様適用。
struct PuckerParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    float amount = 0.0f;
};

/// 伸縮。axis 方向を (1+amount) 倍する(断面は不変)。
struct StretchParams {
    QVector3D center{0.0f, 0.0f, 0.0f};
    int axis = 1;
    float amount = 0.0f;
};

/// ベクトルノイズ。各成分に独立位相の fBm を加算する
/// (displace が法線方向スカラーなのに対しこちらは3方向ランダム)。
struct NoiseParams {
    float amount = 1.0f;
    float frequency = 0.25f;
    int octaves = 3;
    unsigned int seed = 42u;
};

/// ラプラシアン平滑化。隣接頂点平均へ strength だけ寄せる操作を
/// iterations 回反復する。
struct SmoothParams {
    float strength = 0.5f;
    int iterations = 1;
};

/// 成功時 true。position 属性が無い場合は false (mesh は変更されない)。
/// 成功時は computeVertexNormals() 済み。
bool bendMesh(Mesh& mesh, const BendParams& params);
bool twistMesh(Mesh& mesh, const TwistParams& params);
bool taperMesh(Mesh& mesh, const TaperParams& params);
bool displaceMesh(Mesh& mesh, const DisplaceParams& params);
bool waveMesh(Mesh& mesh, const WaveParams& params);
bool bulgeMesh(Mesh& mesh, const BulgeParams& params);
bool spherifyMesh(Mesh& mesh, const SpherifyParams& params);
bool shearMesh(Mesh& mesh, const ShearParams& params);
bool puckerMesh(Mesh& mesh, const PuckerParams& params);
bool stretchMesh(Mesh& mesh, const StretchParams& params);
bool noiseMesh(Mesh& mesh, const NoiseParams& params);
bool smoothMesh(Mesh& mesh, const SmoothParams& params);

} // namespace ArtifactCore
