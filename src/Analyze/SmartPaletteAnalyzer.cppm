module;
#include <utility>
#include <vector>
#include <QImage>
#include <opencv2/opencv.hpp>

module Analyze.SmartPalette;

import Color.Float;
import Image.Raw;

namespace ArtifactCore {

    // ヘルパー関数: CV Matからパレットを抽出
    static std::vector<FloatColor> extractPaletteFromMat(const cv::Mat& mat, int colorCount) {
        if (mat.empty() || colorCount < 1) return {};

        // 1. パフォーマンスのためにリサイズ
        cv::Mat smallMat;
        cv::resize(mat, smallMat, cv::Size(100, 100), 0, 0, cv::INTER_AREA);

        // 2. K-means用にデータを整形
        cv::Mat samples = smallMat.reshape(1, smallMat.rows * smallMat.cols);
        samples.convertTo(samples, CV_32F);

        // 3. K-means実行（クラスタ数の検証）
        cv::Mat labels, centers;
        int actualClusterCount = std::min(colorCount, samples.rows / 2);  // サンプル数の半分以上は不可
        actualClusterCount = std::max(1, actualClusterCount);

        cv::kmeans(samples, actualClusterCount, labels,
            cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0),
            3, cv::KMEANS_PP_CENTERS, centers);

        // 4. クラスタ中心をFloatColorに変換
        std::vector<FloatColor> palette;
        palette.reserve(centers.rows);

        for (int i = 0; i < centers.rows; ++i) {
            float r = 0, g = 0, b = 0, a = 1.0f;

            if (mat.channels() >= 3) {
                // OpenCVのデフォルトはBGR
                b = std::clamp(centers.at<float>(i, 0) / 255.0f, 0.0f, 1.0f);
                g = std::clamp(centers.at<float>(i, 1) / 255.0f, 0.0f, 1.0f);
                r = std::clamp(centers.at<float>(i, 2) / 255.0f, 0.0f, 1.0f);
                if (mat.channels() >= 4) {
                    a = std::clamp(centers.at<float>(i, 3) / 255.0f, 0.0f, 1.0f);
                }
            } else if (mat.channels() == 1) {
                r = g = b = std::clamp(centers.at<float>(i, 0) / 255.0f, 0.0f, 1.0f);
            }

            palette.emplace_back(r, g, b, a);
        }

        return palette;
    }

    std::vector<FloatColor> SmartPaletteAnalyzer::extractPalette(const RawImage& image, int colorCount) {
        if (!image.isValid() || image.data.isEmpty() || image.width <= 0 || image.height <= 0 || colorCount < 1) {
            return {};
        }

        int cvType = CV_8UC3;
        if (image.channels == 4) cvType = CV_8UC4;
        else if (image.channels == 1) cvType = CV_8UC1;

        cv::Mat mat(image.height, image.width, cvType, (void*)image.data.constData());

        return extractPaletteFromMat(mat, colorCount);
    }

    std::vector<FloatColor> SmartPaletteAnalyzer::extractPalette(const QImage& image, int colorCount) {
        if (image.isNull() || colorCount < 1) return {};

        // QImageをOpenCV Matに変換（バッファのコピー所有）
        QImage converted = image.convertToFormat(QImage::Format_RGB888);

        // 重要: cv::Mat が外部バッファを使わず、コピーを所有するように
        cv::Mat mat(converted.height(), converted.width(), CV_8UC3);
        std::memcpy(mat.data, converted.bits(), converted.sizeInBytes());

        return extractPaletteFromMat(mat, colorCount);
    }

}
