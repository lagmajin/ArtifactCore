module;
#include <vector>
#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <opencv2/opencv.hpp>
export module ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

namespace ArtifactCore {

// ストロークのタイプ (前景 / 背景)
export enum class RotoBrushStrokeType {
    Foreground,
    Background
};

// ユーザーがプレビュー画面で描いたストローク情報
export struct RotoBrushStroke {
    RotoBrushStrokeType type;
    std::vector<cv::Point> points;
    int thickness = 5;
};

// ロトブラシのコア機能を担うエンジン
// GrabCutによる初期セグメンテーションと、OpticalFlowによるフレーム間のマスク伝播を提供します。
export class OpenCVRotoBrushEngine {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    OpenCVRotoBrushEngine();
    ~OpenCVRotoBrushEngine();

    // 1. 初回フレームまたはユーザーがストロークを追加/修正したときの更新処理
    // 入力画像と、現在のフレームに描かれたすべてのストロークを元に、アルファマスクを生成します。
    void updateBaseFrame(const cv::Mat& sourceImage, const std::vector<RotoBrushStroke>& strokes);

    // 2. 次のフレームへのトラッキング伝播処理
    // 基準フレーム(baseImage)から次のフレーム(nextImage)へのオプティカルフローを計算し、
    // 前回のマスクを変形させて新しいフレーム用のマスクを高速に推定します。
    void propagateToNextFrame(const cv::Mat& previousImage, const cv::Mat& currentImage);

    // 3. 現在計算されているアルファマスク(0~255)を取得
    cv::Mat getCurrentMask() const;

    // 現在のセッション（トラッキングキャッシュなど）をリセット
    void reset();
};

} // namespace ArtifactCore
