module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>

module FFmpegEncoder.Test;
import Encoder.FFmpegEncoder;
import Image;

namespace ArtifactCore {

/**
 * @brief テスト用カラーパターン生成
 */
ImageF32x4_RGBA generateTestPattern(int frameIndex, int width, int height) {
    cv::Mat mat(height, width, CV_32FC4);
    
    const float t = static_cast<float>(frameIndex) / 100.0f;
    
    for (int y = 0; y < height; ++y) {
        cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
        for (int x = 0; x < width; ++x) {
            // グラデーション＋アニメーション
            const float r = (static_cast<float>(x) / width + std::sin(t * 3.14159f * 2.0f) * 0.3f + 0.5f) * 0.5f;
            const float g = (static_cast<float>(y) / height + std::cos(t * 3.14159f * 2.0f) * 0.3f + 0.5f)  * 0.5f;
            const float b = 0.5f;
            const float a = 1.0f;
            row[x] = cv::Vec4f(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f),
                a);
        }
    }

    ImageF32x4_RGBA image;
    image.setFromCVMat(mat);
    return image;
}

/**
 * @brief PNG シーケンス出力テスト
 */
bool testPngSequenceOutput() {
    qDebug() << "=== FFmpegEncoder Test: PNG Sequence ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(
        QStringLiteral("ffmpeg_test_png_%1/frame_%04d.png").arg(timestamp));
    
    FFmpegImageSequenceSettings settings;
    settings.format = "png";
    settings.width = 1920;
    settings.height = 1080;
    settings.startFrame = 1;
    settings.padding = 4;
    settings.compressionLevel = 6;
    
    FFmpegEncoder encoder;
    
    if (!encoder.openImageSequence(outputPath, settings)) {
        qDebug() << "Failed to open image sequence encoder:" << encoder.lastError();
        return false;
    }
    
    const int numFrames = 10;
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
    }
    
    encoder.close();
    
    // 出力ファイル確認
    for (int i = 1; i <= numFrames; ++i) {
        const QString framePath = QDir(outputDir).filePath(
            QStringLiteral("ffmpeg_test_png_%1/frame_%2.png").arg(timestamp).arg(i, 4, 10, QChar('0')));
        if (!QFile::exists(framePath)) {
            qDebug() << "Frame file does not exist:" << framePath;
            return false;
        }
        QFileInfo fileInfo(framePath);
        qDebug() << "Frame" << i << ":" << fileInfo.size() << "bytes";
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief JPEG シーケンス出力テスト
 */
bool testJpegSequenceOutput() {
    qDebug() << "=== FFmpegEncoder Test: JPEG Sequence ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(
        QStringLiteral("ffmpeg_test_jpg_%1/frame_%04d.jpg").arg(timestamp));
    
    FFmpegImageSequenceSettings settings;
    settings.format = "jpeg";
    settings.width = 1920;
    settings.height = 1080;
    settings.startFrame = 1;
    settings.padding = 4;
    settings.jpegQuality = 85;
    
    FFmpegEncoder encoder;
    
    if (!encoder.openImageSequence(outputPath, settings)) {
        qDebug() << "Failed to open image sequence encoder:" << encoder.lastError();
        return false;
    }
    
    const int numFrames = 5;  // 少なめ
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
    }
    
    encoder.close();
    
    for (int i = 1; i <= numFrames; ++i) {
        const QString framePath = QDir(outputDir).filePath(
            QStringLiteral("ffmpeg_test_jpg_%1/frame_%2.jpg").arg(timestamp).arg(i, 4, 10, QChar('0')));
        if (!QFile::exists(framePath)) {
            qDebug() << "Frame file does not exist:" << framePath;
            return false;
        }
        QFileInfo fileInfo(framePath);
        qDebug() << "Frame" << i << ":" << fileInfo.size() << "bytes";
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief 16bit PNG シーケンス出力テスト
 */
bool test16bitPngSequenceOutput() {
    qDebug() << "=== FFmpegEncoder Test: 16bit PNG Sequence ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(
        QStringLiteral("ffmpeg_test_png16_%1/frame_%04d.png").arg(timestamp));
    
    FFmpegImageSequenceSettings settings;
    settings.format = "png";
    settings.width = 1920;
    settings.height = 1080;
    settings.startFrame = 1;
    settings.padding = 4;
    settings.is16bit = true;  // 16bit 出力
    settings.compressionLevel = 6;
    
    FFmpegEncoder encoder;
    
    if (!encoder.openImageSequence(outputPath, settings)) {
        qDebug() << "Failed to open image sequence encoder:" << encoder.lastError();
        return false;
    }
    
    const int numFrames = 3;  // 少なめ
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
    }
    
    encoder.close();
    
    for (int i = 1; i <= numFrames; ++i) {
        const QString framePath = QDir(outputDir).filePath(
            QStringLiteral("ffmpeg_test_png16_%1/frame_%2.png").arg(timestamp).arg(i, 4, 10, QChar('0')));
        if (!QFile::exists(framePath)) {
            qDebug() << "Frame file does not exist:" << framePath;
            return false;
        }
        QFileInfo fileInfo(framePath);
        qDebug() << "Frame" << i << ":" << fileInfo.size() << "bytes (16bit)";
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief 連番画像フォーマットチェックテスト
 */
bool testImageSequenceFormats() {
    qDebug() << "=== FFmpegEncoder Test: Image Sequence Formats ===";
    
    const QStringList formats = FFmpegEncoder::availableImageSequenceFormats();
    qDebug() << "Available image sequence formats:" << formats;
    
    for (const auto& fmt : formats) {
        if (!FFmpegEncoder::isImageSequenceFormatAvailable(fmt)) {
            qDebug() << "Format" << fmt << "should be available!";
            return false;
        }
    }
    
    // 無効なフォーマット
    if (FFmpegEncoder::isImageSequenceFormatAvailable("invalid_format")) {
        qDebug() << "Invalid format should not be available!";
        return false;
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief H.264 MP4 エンコードテスト（基本）
 */
bool testH264Mp4Encoding() {
    qDebug() << "=== FFmpegEncoder Test: H.264 MP4 ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(QStringLiteral("ffmpeg_test_h264_%1.mp4").arg(timestamp));
    
    qDebug() << "Output path:" << outputPath;
    
    FFmpegEncoderSettings settings;
    settings.width = 1920;
    settings.height = 1080;
    settings.fps = 30.0;
    settings.bitrateKbps = 8000;
    settings.videoCodec = "h264";
    settings.container = "mp4";
    
    FFmpegEncoder encoder;
    
    if (!encoder.open(outputPath, settings)) {
        qDebug() << "Failed to open encoder:" << encoder.lastError();
        return false;
    }
    qDebug() << "Encoder opened successfully";
    
    const int numFrames = 60;
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
        
        if (i % 10 == 0) {
            qDebug() << "Encoded frame" << i << "/" << numFrames;
        }
    }
    
    encoder.close();
    qDebug() << "Encoder closed";
    
    if (!QFile::exists(outputPath)) {
        qDebug() << "Output file does not exist!";
        return false;
    }
    
    QFileInfo fileInfo(outputPath);
    qDebug() << "Output file created:" << fileInfo.filePath();
    qDebug() << "File size:" << fileInfo.size() << "bytes";
    
    if (fileInfo.size() < 1000) {
        qDebug() << "File size is too small, encoding may have failed";
        return false;
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief H.265 MP4 エンコードテスト
 */
bool testH265Mp4Encoding() {
    qDebug() << "=== FFmpegEncoder Test: H.265 MP4 ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(QStringLiteral("ffmpeg_test_h265_%1.mp4").arg(timestamp));
    
    qDebug() << "Output path:" << outputPath;
    
    FFmpegEncoderSettings settings;
    settings.width = 1920;
    settings.height = 1080;
    settings.fps = 30.0;
    settings.bitrateKbps = 8000;
    settings.videoCodec = "h265";
    settings.container = "mp4";
    settings.crf = 28;  // H.265 は大きめ CRF でも高品質
    
    FFmpegEncoder encoder;
    
    if (!encoder.open(outputPath, settings)) {
        qDebug() << "Failed to open encoder:" << encoder.lastError();
        // H.265 が利用できない環境もあるのでスキップ
        if (!FFmpegEncoder::isCodecAvailable("h265")) {
            qDebug() << "H.265 codec not available, skipping test";
            return true;  // テスト成功として扱う
        }
        return false;
    }
    
    const int numFrames = 30;  // 短め
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
    }
    
    encoder.close();
    
    if (!QFile::exists(outputPath)) {
        qDebug() << "Output file does not exist!";
        return false;
    }
    
    QFileInfo fileInfo(outputPath);
    qDebug() << "H.265 output created:" << fileInfo.size() << "bytes";
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief ProRes MOV エンコードテスト
 */
bool testProresMovEncoding() {
    qDebug() << "=== FFmpegEncoder Test: ProRes MOV ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QDir(outputDir).filePath(QStringLiteral("ffmpeg_test_prores_%1.mov").arg(timestamp));
    
    FFmpegEncoderSettings settings;
    settings.width = 1920;
    settings.height = 1080;
    settings.fps = 24.0;  // シネマティック
    settings.bitrateKbps = 50000;  // ProRes は高ビットレート
    settings.videoCodec = "prores";
    settings.container = "mov";
    settings.profile = "hq";  // ProRes HQ
    
    FFmpegEncoder encoder;
    
    if (!encoder.open(outputPath, settings)) {
        qDebug() << "Failed to open encoder:" << encoder.lastError();
        if (!FFmpegEncoder::isCodecAvailable("prores")) {
            qDebug() << "ProRes codec not available, skipping test";
            return true;
        }
        return false;
    }
    
    const int numFrames = 24;  // 1 秒分
    for (int i = 0; i < numFrames; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
        if (!encoder.addImage(frame)) {
            qDebug() << "Failed to add frame" << i << ":" << encoder.lastError();
            encoder.close();
            return false;
        }
    }
    
    encoder.close();
    
    if (!QFile::exists(outputPath)) {
        qDebug() << "Output file does not exist!";
        return false;
    }
    
    QFileInfo fileInfo(outputPath);
    qDebug() << "ProRes output created:" << fileInfo.size() << "bytes";
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief 利用可能コーデックチェックテスト
 */
bool testAvailableCodecs() {
    qDebug() << "=== FFmpegEncoder Test: Available Codecs ===";
    
    const QStringList codecs = FFmpegEncoder::availableVideoCodecs();
    qDebug() << "Available video codecs:" << codecs;
    
    const QStringList containers = FFmpegEncoder::availableContainers();
    qDebug() << "Available containers:" << containers;
    
    // 少なくとも H.264 は利用可能であるべき
    if (!FFmpegEncoder::isCodecAvailable("h264")) {
        qDebug() << "H.264 codec should be available!";
        return false;
    }
    
    if (!FFmpegEncoder::isContainerAvailable("mp4")) {
        qDebug() << "MP4 container should be available!";
        return false;
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief CRF 品質設定テスト
 */
bool testCrfSettings() {
    qDebug() << "=== FFmpegEncoder Test: CRF Settings ===";
    
    const QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    
    // 異なる CRF 値でエンコード
    for (int crf : {18, 23, 28}) {
        const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString outputPath = QDir(outputDir).filePath(
            QStringLiteral("ffmpeg_test_crf%1_%2.mp4").arg(crf).arg(timestamp));
        
        FFmpegEncoderSettings settings;
        settings.width = 1920;
        settings.height = 1080;
        settings.fps = 30.0;
        settings.videoCodec = "h264";
        settings.container = "mp4";
        settings.crf = crf;
        settings.preset = "fast";  // 高速
        
        FFmpegEncoder encoder;
        
        if (!encoder.open(outputPath, settings)) {
            qDebug() << "Failed to open encoder with CRF" << crf << ":" << encoder.lastError();
            return false;
        }
        
        for (int i = 0; i < 10; ++i) {
            ImageF32x4_RGBA frame = generateTestPattern(i, settings.width, settings.height);
            encoder.addImage(frame);
        }
        
        encoder.close();
        
        if (!QFile::exists(outputPath)) {
            qDebug() << "Output file does not exist for CRF" << crf;
            return false;
        }
        
        QFileInfo fileInfo(outputPath);
        qDebug() << "CRF" << crf << "output:" << fileInfo.size() << "bytes";
    }
    
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief エラーケーステスト：無効なコーデック
 */
bool testInvalidCodec() {
    qDebug() << "=== FFmpegEncoder Test: Invalid Codec ===";
    
    const QString outputPath = QDir::temp().filePath("test_invalid.mp4");
    
    FFmpegEncoderSettings settings;
    settings.videoCodec = "invalid_codec_name";
    settings.container = "mp4";
    
    FFmpegEncoder encoder;
    
    if (encoder.open(outputPath, settings)) {
        qDebug() << "Should have failed with invalid codec";
        encoder.close();
        return false;
    }
    
    qDebug() << "Correctly rejected invalid codec:" << encoder.lastError();
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief エラーケーステスト：無効なコンテナ
 */
bool testInvalidContainer() {
    qDebug() << "=== FFmpegEncoder Test: Invalid Container ===";
    
    const QString outputPath = QDir::temp().filePath("test_invalid.xxx");
    
    FFmpegEncoderSettings settings;
    settings.videoCodec = "h264";
    settings.container = "invalid_container";
    
    FFmpegEncoder encoder;
    
    if (encoder.open(outputPath, settings)) {
        qDebug() << "Should have failed with invalid container";
        encoder.close();
        return false;
    }
    
    qDebug() << "Correctly rejected invalid container:" << encoder.lastError();
    qDebug() << "=== Test PASSED ===";
    return true;
}

/**
 * @brief 全テスト実行
 */
int runAllTests() {
    int passed = 0;
    int total = 0;

    // 基本機能テスト
    total++; if (testAvailableCodecs()) passed++;
    total++; if (testImageSequenceFormats()) passed++;
    
    // ビデオエンコードテスト
    total++; if (testH264Mp4Encoding()) passed++;
    total++; if (testH265Mp4Encoding()) passed++;
    total++; if (testProresMovEncoding()) passed++;
    
    // 連番画像テスト
    total++; if (testPngSequenceOutput()) passed++;
    total++; if (testJpegSequenceOutput()) passed++;
    total++; if (test16bitPngSequenceOutput()) passed++;
    
    // 設定テスト
    total++; if (testCrfSettings()) passed++;
    
    // エラーケーステスト
    total++; if (testInvalidCodec()) passed++;
    total++; if (testInvalidContainer()) passed++;

    qDebug() << "";
    qDebug() << "=== Test Summary ===";
    qDebug() << "Passed:" << passed << "/" << total;

    return (passed == total) ? 0 : 1;
}

} // namespace ArtifactCore

// エントリーポイント（スタンドアロン実行用）
#ifdef STANDALONE_TEST
int main() {
    return ArtifactCore::runAllTests();
}
#endif
