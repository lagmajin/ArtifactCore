module;

#include <QFile>
#include <QString>
#include <QStringList>

export module Encoder.FFmpegEncoder;

//import EncoderSettings;

import Image;


export namespace ArtifactCore {

    // FFmpeg エンコーダー設定
    struct FFmpegEncoderSettings {
        int width = 1920;
        int height = 1080;
        double fps = 30.0;
        int bitrateKbps = 8000;
        QString videoCodec = "h264";
        QString container = "mp4";
        QString encoderName;

        // コーデック固有オプション
        QString preset = "medium";      // ultrafast, fast, medium, slow, veryslow
        int crf = 23;                   // 品質（0-51, 低ほど高品質）
        int gopSize = 10;               // キーフレーム間隔
        int maxBFrames = 0;             // B フレーム数
        QString profile = "high";       // コーデックプロファイル
        bool zerolatency = true;        // ゼロレイテンシモード
    };

    // 連番画像出力設定
    struct FFmpegImageSequenceSettings {
        QString format = "png";         // png, jpeg, exr, tiff, bmp
        int width = 1920;
        int height = 1080;
        int startFrame = 1;             // 開始フレーム番号
        int padding = 4;                // ゼロ埋め桁数（4 なら 0001）
        int jpegQuality = 90;           // JPEG 品質（1-100）
        bool is16bit = false;           // 16bit 出力（PNG/TIFF）
        bool is32bit = false;           // 32bit float 出力（EXR）
        int compressionLevel = 6;       // 圧縮レベル（PNG:0-9, TIFF:1-9）
    };

    //ffmpeg encoder
    class FFmpegEncoder {
    private:
        class Impl;
        Impl* impl_;
    public:
        FFmpegEncoder();
        ~FFmpegEncoder();

        FFmpegEncoder(const FFmpegEncoder&) = delete;
        FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;

        // ビデオエンコーダー初期化
        bool open(const QString& outputPath, const FFmpegEncoderSettings& settings = FFmpegEncoderSettings());

        // 連番画像エンコーダー初期化
        bool openImageSequence(const QString& outputPathPattern, const FFmpegImageSequenceSettings& settings = FFmpegImageSequenceSettings());

        // フレーム追加
        bool addImage(const ImageF32x4_RGBA& image);

        // エンコード完了とファイルクローズ
        void close();

        // 最後のエラーメッセージ取得
        QString lastError() const;

        // 有効かどうか
        bool isOpen() const;

        // 連番画像出力中か
        bool isImageSequence() const;

        // 静的ヘルパー：コーデックが利用可能かチェック
        static bool isCodecAvailable(const QString& codecName);
        static bool isEncoderAvailable(const QString& encoderName);

        // 静的ヘルパー：利用可能なコーデック一覧取得
        static QStringList availableVideoCodecs();
        static QStringList availableHardwareVideoEncoders();

        // 静的ヘルパー：コンテナ形式が利用可能かチェック
        static bool isContainerAvailable(const QString& containerName);

        // 静的ヘルパー：利用可能なコンテナ形式一覧取得
        static QStringList availableContainers();

        // 静的ヘルパー：連番画像フォーマットが利用可能かチェック
        static bool isImageSequenceFormatAvailable(const QString& format);

        // 静的ヘルパー：利用可能な連番画像フォーマット一覧
        static QStringList availableImageSequenceFormats();
    };


};
