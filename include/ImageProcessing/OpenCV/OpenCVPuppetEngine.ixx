module;
#include <vector>
#include <memory>
#include <string>
#include <opencv2/opencv.hpp>

export module ArtifactCore.ImageProcessing.OpenCV.PuppetEngine;

import std;

namespace ArtifactCore {

// パペットピンのデータ構造
// 初期位置(originalPosition)と、ユーザーがドラッグして移動させた現在位置(currentPosition)を保持します。
export struct PuppetPin {
    std::string id;             // ピンの一意識別子
    cv::Point2f originalPosition; // キャッシュ/バインド時の初期座標
    cv::Point2f currentPosition;  // 移動後の座標
    float weight = 1.0f;          // 変形に対する影響力（将来的な拡張用）
};

// パペットメッシュの頂点およびポリゴン情報（将来的なハードウェアレンダリング用）
export struct PuppetMesh {
    std::vector<cv::Point2f> vertices;   // 変形後の頂点座標
    std::vector<cv::Point2f> texCoords;  // 元画像のUV座標(0.0~1.0またはピクセル座標)
    std::vector<int> indices;            // 三角形ポリゴンのインデックスリスト (v1, v2, v3, ...)
};

// パペット変形アルゴリズム (Moving Least Squares / As-Rigid-As-Possible / Thin Plate Spline 等)
export enum class PuppetDeformationMethod {
    ThinPlateSpline,    // スムーズな曲面変形 (OpenCV標準ベース)
    MovingLeastSquares, // 剛性を保ちやすい2D変形 (MLS: アフィン/剛体変換)
    ARAP                // As-Rigid-As-Possible (より高度な形状保持メッシュ変形)
};

// パペットツールのコアエンジン基盤
// 画像領域のメッシュ化と、ピン（コントロールポイント）に基づく画像/メッシュのワープ（変形）を提供します。
export class OpenCVPuppetEngine {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    OpenCVPuppetEngine();
    ~OpenCVPuppetEngine();

    // 1. 初期化とメッシュ生成
    // 入力画像（アルファチャンネル付き）から被写体の輪郭を抽出し、
    // ドロネー(Delaunay)網掛けを行ってコントロール用の初期メッシュを生成します。
    // detailLevel: メッシュの細かさ (値が大きいほど細かい)
    void bindImage(const cv::Mat& sourceImage, int detailLevel = 10);

    // 2. ピンの追加・削除
    // メッシュ上の座標にコントロールピンを配置/削除します。
    void addPin(const PuppetPin& pin);
    void removePin(const std::string& pinId);
    void updatePinPosition(const std::string& pinId, const cv::Point2f& newPosition);
    
    // 現在登録されているすべてのピンを取得
    std::vector<PuppetPin> getPins() const;

    // 3. 変形の計算と適用
    // サポートされているアルゴリズム(MLS/TPSなど)を用いて画像のワープ処理を行います。
    // CPUで処理された変形後の画像を返します。
    cv::Mat renderDeformedImage(PuppetDeformationMethod method = PuppetDeformationMethod::MovingLeastSquares);

    // 4. メッシュ情報の取得 (GPUレンダリング用)
    // 画像全体をCPUでワープさせるのではなく、変形後のメッシュ頂点情報のみを返し、
    // UI側のOpenGL/DirectX等でテクスチャマッピングとして描画するための高度なパイプライン用インターフェースです。
    PuppetMesh getDeformedMesh() const;

    // 状態をリセット
    void reset();
};

} // namespace ArtifactCore
