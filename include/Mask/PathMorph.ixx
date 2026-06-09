module;
#include <vector>
#include <QPointF>
#include <QPainterPath>
export module Core.Mask.PathMorph;

export namespace ArtifactCore {

/// 頂点対応付け方式
enum class MorphCorrespondence {
    Ordered,     ///< 同じインデックス同士を対応（サブパス数・頂点数が一致前提）
    Equidistant  ///< 両パスを同数で等距離サンプリングして対応
};

/// パスモーフィング設定
struct PathMorphSettings {
    int sampleCount = 64;                       ///< 等距離サンプリング点数
    MorphCorrespondence correspondence = MorphCorrespondence::Equidistant;
    bool topologyLock = true;                   ///< true: subpath の数と開閉状態を保持
};

/// パス補間の途中結果（点列）
struct MorphPathSample {
    std::vector<std::vector<QPointF>> subpaths; ///< 各 subpath のサンプル点列
    bool closed;                                 ///< 全体として閉じているか
};

/// 2つの QPainterPath 間でスムーズなモーフィングを行う
class PathMorphEngine {
public:
    /// パスA→Bを t (0..1) で補間した QPainterPath を返す
    static QPainterPath interpolate(
        const QPainterPath& pathA,
        const QPainterPath& pathB,
        float t,
        const PathMorphSettings& settings = {});

    /// パスを等距離サンプリングした点列に分解
    static MorphPathSample samplePath(
        const QPainterPath& path,
        int pointsPerSubpath);

    /// 点列同士をサブパスごとに補間
    static QPainterPath interpolateSamples(
        const MorphPathSample& sampleA,
        const MorphPathSample& sampleB,
        float t);

    /// 面積＋重心位置でサブパスの対応付けを行ってから補間。
    /// サブパス数が異なる場合でも最も近い特徴量同士でマッチングする。
    static QPainterPath interpolateByFeature(
        const QPainterPath& pathA,
        const QPainterPath& pathB,
        float t,
        int sampleCount = 64);
};

}
