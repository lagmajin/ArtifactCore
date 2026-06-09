module;
#include <vector>
#include <opencv2/opencv.hpp>
export module Core.Mask.VolumeMask;

export namespace ArtifactCore {

/// ボリューム減衰カーブ
enum class VolumeFalloffCurve {
    Step,     // 境界で不透明度が直接 density に
    Linear,   // 線形増加
    EaseIn,   // 二次曲線（ゆっくり立ち上がる）
    EaseOut   // 二次曲線（最後まで立ち上がる）
};

/// ボリュームマスクの設定
struct VolumeMaskSettings {
    float thickness = 50.0f;                ///< マスク境界からの内側の厚み（ピクセル）
    float density = 0.8f;                   ///< 最大不透明度（0〜1）
    VolumeFalloffCurve falloffCurve = VolumeFalloffCurve::Linear;
    bool invert = false;                    ///< 内外反転
};

/// 閉じたポリゴン輪郭から Signed Distance Field を生成し、
/// 厚みと減衰を適用したソフトボリュームマスクを計算する
class VolumeMaskGenerator {
public:
    /// 閉じたポリゴン輪郭から符号付き距離場（CV_32FC1）を生成
    /// contour: 整数ピクセル座標の閉じた輪郭
    /// 内部が正の距離、外部が負の距離
    static cv::Mat generateDistanceField(
        const std::vector<cv::Point>& contour,
        int width, int height
    );

    /// 距離場と設定からαマスク（CV_32FC1）を計算
    /// distanceField: generateDistanceField() の出力
    static cv::Mat computeAlphaMask(
        const cv::Mat& distanceField,
        const VolumeMaskSettings& settings
    );

    /// ワンステップ：輪郭→αマスク
    static void renderToAlpha(
        const std::vector<cv::Point>& contour,
        int width, int height,
        void* outMat,               ///< 出力 CV_32FC1 行列
        const VolumeMaskSettings& settings
    );
};

}
