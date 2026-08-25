module;

#include <vector>
#include <QPoint>

export module Geometry.ShapeExtrude;

import Mesh;

export namespace ArtifactCore {

/// 輪郭押し出しパラメータ（Element 3D 風の Extrude + Bevel）
struct ShapeExtrudeParams {
    /// 押し出しの全長（Z 軸、[-depth/2, depth/2] に対称配置）
    float depth = 40.0f;
    /// ベベル幅（輪郭内側への寄せ量）。0 でベベル無し
    float bevelWidth = 4.0f;
    /// ベベルの分割数（円弧近似）。1 で直角チャンファー
    int bevelSegments = 3;
};

/// 閉じた輪郭群から押し出しメッシュを生成する。
///
/// contours は XY 平面上の閉じたポリライン（終点=始点でも可）。
/// even-odd 規則で内側の輪郭は穴として扱う。
/// 生成される Mesh は position / normal / uv 頂点属性を持ち、
/// キャップ・ベベル・側壁が四角形 / 三角形ポリゴンとして登録される。
/// Model3D レイヤーの GPU render path（generateRenderData 経由）で
/// そのまま描画できる形式。
///
/// 戻り値: メッシュを生成できた場合 true。輪郭が不正・退化している場合は false
/// （outMesh は変更されない）。
bool extrudeContourMesh(const std::vector<std::vector<QPointF>>& contours,
                        const ShapeExtrudeParams& params,
                        Mesh& outMesh);

} // namespace ArtifactCore
