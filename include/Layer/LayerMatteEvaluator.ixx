module;
#include <utility>
#include <vector>
#include <QImage>
#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

export module Layer.MatteEvaluator;

import Layer.Matte;
import Utils.Id;

export namespace ArtifactCore {

/**
 * @brief マット評価パイプライン
 *
 * 1つ以上のマット参照を受け取り、最終的なアルファマスクを生成する。
 * AEのトラックマット+マスクスタックに相当する機能。
 */
class LayerMatteEvaluator {
public:
    /**
     * @brief 単一マットソースからアルファ/ルミナンスチャンネルを抽出
     * @param source マットソース画像（RGBA または RGB）
     * @param type マット抽出タイプ（Alpha/Luma/Inverse等）
     * @return 正規化された 0.0-1.0 の float マスク（1チャンネル）
     */
    static cv::Mat extractMatteChannel(const cv::Mat& source, MatteType type);

    /**
     * @brief 2つのマットを指定モードで合成
     * @param base 既存のマスク
     * @param overlay 追加するマスク
     * @param mode 合成モード
     * @return 合成後のマスク
     */
    static cv::Mat blendMatteChannels(const cv::Mat& base, const cv::Mat& overlay, MatteBlendMode mode);

    /**
     * @brief マットスタックを一括評価
     * @param matteSources マットソース画像のリスト（評価順序順）
     * @param matteTypes 各マットの抽出タイプ
     * @param blendModes 各マットの合成モード（先頭はベース、2番目以降が基準）
     * @param opacities 各マットの不透明度（0.0-1.0）
     * @param inverts 各マットの反転フラグ
     * @param targetSize 出力マスクのサイズ
     * @return 最終合成マスク（CV_32FC1、0.0-1.0）
     */
    static cv::Mat evaluateMatteStack(
        const std::vector<cv::Mat>& matteSources,
        const std::vector<MatteType>& matteTypes,
        const std::vector<MatteBlendMode>& blendModes,
        const std::vector<float>& opacities,
        const std::vector<bool>& inverts,
        const cv::Size& targetSize);

    /**
     * @brief マット画像をターゲットサイズにフィットさせる
     * @param source マットソース画像
     * @param targetSize ターゲットサイズ
     * @param fitMode フィットモード
     * @return リサイズ/配置済みの画像
     */
    static cv::Mat fitMatteToTarget(const cv::Mat& source, const cv::Size& targetSize, MatteFitMode fitMode);

    /**
     * @brief 評価済みマットをターゲット画像のアルファチャンネルに適用
     * @param target ターゲット画像（RGBA、CV_8UC4 または CV_32FC4）
     * @param matte 評価済みマット（CV_32FC1、0.0-1.0）
     * @return マット適用後の画像
     */
    static cv::Mat applyMatteToImage(const cv::Mat& target, const cv::Mat& matte);

    /**
     * @brief QImage から cv::Mat へ変換（評価用）
     */
    static cv::Mat qImageToMat(const QImage& image);

    /**
     * @brief cv::Mat から QImage へ変換（結果取得用）
     */
    static QImage matToQImage(const cv::Mat& mat);
};

} // namespace ArtifactCore
