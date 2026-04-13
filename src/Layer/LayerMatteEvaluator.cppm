module;
#include <utility>
#include <vector>
#include <algorithm>
#include <QImage>
#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

module Layer.MatteEvaluator;

import std;
import Layer.Matte;

namespace ArtifactCore {

cv::Mat LayerMatteEvaluator::extractMatteChannel(const cv::Mat& source, MatteType type) {
    CV_Assert(!source.empty());

    cv::Mat matte;
    int channels = source.channels();

    switch (type) {
    case MatteType::Alpha:
    case MatteType::InverseAlpha:
        if (channels >= 4) {
            // RGBA → Alpha チャンネル抽出
            std::vector<cv::Mat> chan;
            cv::split(source, chan);
            chan[3].convertTo(matte, CV_32FC1, 1.0 / 255.0);
        } else {
            // RGB の場合は全ピクセル 1.0（不透明）として扱う
            matte = cv::Mat::ones(source.size(), CV_32FC1);
        }
        break;

    case MatteType::Luma:
    case MatteType::InverseLuma: {
        cv::Mat rgb;
        if (channels == 4) {
            cv::cvtColor(source, rgb, cv::COLOR_BGRA2BGR);
        } else if (channels == 3) {
            rgb = source;
        } else {
            cv::cvtColor(source, rgb, cv::COLOR_GRAY2BGR);
        }
        // BT.709 輝度変換
        std::vector<cv::Mat> bgr;
        cv::split(rgb, bgr);
        matte = 0.0722f * bgr[0] + 0.7152f * bgr[1] + 0.2126f * bgr[2];
        matte.convertTo(matte, CV_32FC1, 1.0 / 255.0);
        break;
    }
    }

    // Invert 処理
    if (type == MatteType::InverseAlpha || type == MatteType::InverseLuma) {
        matte = 1.0f - matte;
    }

    // クランプ
    cv::threshold(matte, matte, 0.0f, 0.0f, cv::THRESH_TOZERO);
    cv::threshold(matte, matte, 1.0f, 1.0f, cv::THRESH_TRUNC);

    return matte;
}

cv::Mat LayerMatteEvaluator::blendMatteChannels(const cv::Mat& base, const cv::Mat& overlay, MatteBlendMode mode) {
    CV_Assert(!base.empty());
    CV_Assert(!overlay.empty());
    CV_Assert(base.size() == overlay.size());

    cv::Mat result;

    switch (mode) {
    case MatteBlendMode::Add:
        // 最大値合成（明るい方を優先）
        cv::max(base, overlay, result);
        break;

    case MatteBlendMode::Subtract:
        // 減算（overlay 部分をマスクから除去）
        result = base.mul(1.0f - overlay);
        break;

    case MatteBlendMode::Intersect:
        // 積算（両方が覆っている部分のみ残す）
        result = base.mul(overlay);
        break;

    case MatteBlendMode::Difference:
        // 差の絶対値
        result = cv::abs(base - overlay);
        break;

    default:
        result = base.clone();
        break;
    }

    return result;
}

cv::Mat LayerMatteEvaluator::evaluateMatteStack(
    const std::vector<cv::Mat>& matteSources,
    const std::vector<MatteType>& matteTypes,
    const std::vector<MatteBlendMode>& blendModes,
    const std::vector<float>& opacities,
    const std::vector<bool>& inverts,
    const cv::Size& targetSize)
{
    if (matteSources.empty() || matteTypes.empty()) {
        return cv::Mat::ones(targetSize, CV_32FC1);
    }

    size_t count = matteSources.size();
    cv::Mat result;

    for (size_t i = 0; i < count; ++i) {
        // 1. フィット
        cv::Mat fitted = fitMatteToTarget(matteSources[i], targetSize, MatteFitMode::Stretch);

        // 2. チャンネル抽出
        MatteType type = (i < matteTypes.size()) ? matteTypes[i] : MatteType::Alpha;
        cv::Mat matte = extractMatteChannel(fitted, type);

        // 3. リサイズ（ターゲットサイズに合わせる）
        if (matte.size() != targetSize) {
            cv::resize(matte, matte, targetSize, 0, 0, cv::INTER_LINEAR);
        }

        // 4. Invert
        if (i < inverts.size() && inverts[i]) {
            matte = 1.0f - matte;
        }

        // 5. Opacity 適用
        if (i < opacities.size()) {
            matte *= opacities[i];
        }

        // 6. 合成
        if (i == 0) {
            result = matte;
        } else {
            MatteBlendMode mode = (i < blendModes.size()) ? blendModes[i] : MatteBlendMode::Add;
            result = blendMatteChannels(result, matte, mode);
        }
    }

    // 最終クランプ
    if (!result.empty()) {
        cv::threshold(result, result, 0.0f, 0.0f, cv::THRESH_TOZERO);
        cv::threshold(result, result, 1.0f, 1.0f, cv::THRESH_TRUNC);
    }

    return result;
}

cv::Mat LayerMatteEvaluator::fitMatteToTarget(const cv::Mat& source, const cv::Size& targetSize, MatteFitMode fitMode) {
    CV_Assert(!source.empty());

    if (source.size() == targetSize) {
        return source.clone();
    }

    cv::Mat result;

    switch (fitMode) {
    case MatteFitMode::Stretch:
        cv::resize(source, result, targetSize, 0, 0, cv::INTER_LINEAR);
        break;

    case MatteFitMode::Fit:
    case MatteFitMode::Fill: {
        double srcAspect = static_cast<double>(source.cols) / source.rows;
        double dstAspect = static_cast<double>(targetSize.width) / targetSize.height;
        int resizeWidth, resizeHeight;

        if (fitMode == MatteFitMode::Fit) {
            if (srcAspect > dstAspect) {
                resizeWidth = targetSize.width;
                resizeHeight = static_cast<int>(targetSize.width / srcAspect);
            } else {
                resizeHeight = targetSize.height;
                resizeWidth = static_cast<int>(targetSize.height * srcAspect);
            }
        } else { // Fill
            if (srcAspect > dstAspect) {
                resizeHeight = targetSize.height;
                resizeWidth = static_cast<int>(targetSize.height * srcAspect);
            } else {
                resizeWidth = targetSize.width;
                resizeHeight = static_cast<int>(targetSize.width / srcAspect);
            }
        }

        cv::Mat resized;
        cv::resize(source, resized, cv::Size(resizeWidth, resizeHeight), 0, 0, cv::INTER_LINEAR);

        // 黒背景でパディング
        result = cv::Mat::zeros(targetSize, source.type());
        int offsetX = (targetSize.width - resizeWidth) / 2;
        int offsetY = (targetSize.height - resizeHeight) / 2;
        resized.copyTo(result(cv::Rect(std::max(0, offsetX), std::max(0, offsetY),
                                        std::min(resizeWidth, targetSize.width),
                                        std::min(resizeHeight, targetSize.height))));
        break;
    }

    case MatteFitMode::Original: {
        result = cv::Mat::zeros(targetSize, source.type());
        int offsetX = (targetSize.width - source.cols) / 2;
        int offsetY = (targetSize.height - source.rows) / 2;
        source.copyTo(result(cv::Rect(std::max(0, offsetX), std::max(0, offsetY),
                                       std::min(source.cols, targetSize.width),
                                       std::min(source.rows, targetSize.height))));
        break;
    }
    }

    return result;
}

cv::Mat LayerMatteEvaluator::applyMatteToImage(const cv::Mat& target, const cv::Mat& matte) {
    CV_Assert(!target.empty());
    CV_Assert(!matte.empty());
    CV_Assert(target.size() == matte.size());

    cv::Mat result;

    if (target.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(target, channels);

        // アルファチャンネルにマットを乗算
        cv::Mat alphaF;
        channels[3].convertTo(alphaF, CV_32FC1, 1.0 / 255.0);
        alphaF = alphaF.mul(matte);
        alphaF.convertTo(channels[3], CV_8UC1, 255.0);

        cv::merge(channels, result);
    } else if (target.channels() == 3) {
        // RGB の場合はアルファチャンネルを追加
        std::vector<cv::Mat> channels;
        cv::split(target, channels);
        cv::Mat matte8U;
        matte.convertTo(matte8U, CV_8UC1, 255.0);
        channels.push_back(matte8U);
        cv::merge(channels, result);
    } else {
        result = target.clone();
    }

    return result;
}

cv::Mat LayerMatteEvaluator::qImageToMat(const QImage& image) {
    if (image.isNull()) return cv::Mat();

    QImage converted = image.convertToFormat(QImage::Format_ARGB32);
    cv::Mat mat(converted.height(), converted.width(), CV_8UC4);

    for (int y = 0; y < converted.height(); ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(converted.scanLine(y));
        cv::Vec4b* dstLine = mat.ptr<cv::Vec4b>(y);
        for (int x = 0; x < converted.width(); ++x) {
            dstLine[x] = cv::Vec4b(
                qBlue(srcLine[x]),
                qGreen(srcLine[x]),
                qRed(srcLine[x]),
                qAlpha(srcLine[x])
            );
        }
    }

    return mat;
}

QImage LayerMatteEvaluator::matToQImage(const cv::Mat& mat) {
    if (mat.empty()) return QImage();

    cv::Mat display;
    if (mat.channels() == 1) {
        cv::cvtColor(mat, display, cv::COLOR_GRAY2BGRA);
    } else if (mat.channels() == 3) {
        cv::cvtColor(mat, display, cv::COLOR_BGR2BGRA);
    } else {
        display = mat;
    }

    QImage image(display.cols, display.rows, QImage::Format_ARGB32);
    for (int y = 0; y < display.rows; ++y) {
        const cv::Vec4b* srcLine = display.ptr<cv::Vec4b>(y);
        QRgb* dstLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < display.cols; ++x) {
            dstLine[x] = qRgba(
                srcLine[x][2],
                srcLine[x][1],
                srcLine[x][0],
                srcLine[x][3]
            );
        }
    }

    return image;
}

} // namespace ArtifactCore
