module;

#include <QString>
#include <QObject>
#include <QImage>
#include <QColor>
#include <vector>
#include <memory>

export module Audio.WaveformGenerator;

import std;

/**
 * @brief Waveform Generator
 * 
 * オーディオ波形生成クラス
 * タイムライン表示用のオーディオ波形ビジュアルを生成
 */
export namespace ArtifactCore {

    /// @brief 波形タイプ
    enum class WaveformType {
        Bars,          // 棒グラフ
        Lines,         // ライン
        Filled,        // 塗りつぶし
        Mirrored,      // ミラー表示
    };

    /// @brief 波形スタイル
    enum class WaveformStyle {
        Classic,       // クラシック
        Modern,        // モダンス
        Minimal,       // ミニマル
    };

    /// @brief 波形設定
    struct WaveformSettings {
        WaveformType type = WaveformType::Bars;
        WaveformStyle style = WaveformStyle::Modern;
        
        // 表示設定
        int height = 100;                    // 波形表示高さ
        int samplesPerPixel = 256;           // ピクセルあたりのサンプル数
        bool showRMS = true;                  // RMS也表示
        bool mirror = true;                  // ミラー表示
        
        // 颜色
        QColor primaryColor = QColor("#00A8E8");    // メインカラー
        QColor secondaryColor = QColor("#007EA7");  // セカンダリ（ RMS等）
        QColor backgroundColor = QColor("#1A1A2E"); // 背景色
        QColor peakColor = QColor("#FFFFFF");       // ピークカラー
        
        // 詳細設定
        bool showPeaks = true;                // ピーク表示
        bool gradient = true;                 // グラデーション
        int barSpacing = 1;                   // 棒間隔
        bool autoScale = true;                // 自動スケール
        
        // 品質
        float quality = 1.0f;                 // 品質（0.0-1.0）
    };

    /// @brief 波形データ
    struct WaveformData {
        std::vector<float> minValues;        // 最小値
        std::vector<float> maxValues;        // 最大値
        std::vector<float> rmsValues;        // RMS値
        std::vector<float> peakValues;       // ピーク値
        
        int sampleRate = 48000;              // サンプルレート
        int channelCount = 2;                // チャンネル数
        int64_t totalSamples = 0;            // 総サンプル数
        float duration = 0.0f;               // 長さ（秒）
        
        bool isValid() const { return !minValues.empty(); }
    };

    /// @brief 波形生成結果
    struct WaveformResult {
        QImage image;                         // 生成された画像
        bool success = false;                 // 成功か
        QString errorMessage;                 // エラーメッセージ
        int width = 0;                        // 画像幅
        int height = 0;                       // 画像高さ
    };

    /// @brief オーディオ波形生成クラス
    export class WaveformGenerator : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief コンストラクタ
         * @param parent 親オブジェクト
         */
        explicit WaveformGenerator(QObject* parent = nullptr);
        
        ~WaveformGenerator() override;

        // コピー・ムーブ禁止
        WaveformGenerator(const WaveformGenerator&) = delete;
        WaveformGenerator& operator=(const WaveformGenerator&) = delete;

        // ===== 設定 =====

        /// @brief 波形タイプを設定
        void setWaveformType(WaveformType type);
        WaveformType getWaveformType() const { return settings_.type; }

        /// @brief 波形スタイルを設定
        void setWaveformStyle(WaveformStyle style);
        WaveformStyle getWaveformStyle() const { return settings_.style; }

        /// @brief 波形設定整体を設定
        void setSettings(const WaveformSettings& settings);
        const WaveformSettings& getSettings() const { return settings_; }

        /// @brief 表示高さを設定
        void setHeight(int height);
        int getHeight() const { return settings_.height; }

        /// @brief ピクセルあたりのサンプル数を設定
        void setSamplesPerPixel(int samples);
        int getSamplesPerPixel() const { return settings_.samplesPerPixel; }

        /// @brief 颜色を設定
        void setPrimaryColor(const QColor& color);
        void setSecondaryColor(const QColor& color);
        void setBackgroundColor(const QColor& color);

        // ===== 波形生成 =====

        /// @brief 波形データを生成（生データから）
        /// @param samples サンプルデータ（ステレオの場合はLR交互）
        /// @param sampleCount サンプル数
        /// @param sampleRate サンプルレート
        /// @param channels チャンネル数
        /// @return 波形データ
        WaveformData generateData(const float* samples, size_t sampleCount, 
                                 int sampleRate = 48000, int channels = 2);

        /// @brief 波形データを生成（ファイルから）
        /// @param audioFilePath オーディオファイルパス
        /// @return 波形データ
        WaveformData generateDataFromFile(const QString& audioFilePath);

        /// @brief 波形画像を生成（波形データから）
        /// @param data 波形データ
        /// @param width 出力幅
        /// @return 波形画像
        WaveformResult generateImage(const WaveformData& data, int width);

        /// @brief 波形画像を生成（生データから）
        /// @param samples サンプルデータ
        /// @param sampleCount サンプル数
        /// @param width 出力幅
        /// @return 波形画像
        WaveformResult generateImage(const float* samples, size_t sampleCount, 
                                    int width, int sampleRate = 48000, int channels = 2);

        /// @brief 波形画像を生成（ファイルから）
        /// @param audioFilePath オーディオファイルパス
        /// @param width 出力幅
        /// @return 波形画像
        WaveformResult generateImageFromFile(const QString& audioFilePath, int width);

        // ===== キャッシュ =====

        /// @brief 波形データをキャッシュ
        void cacheWaveformData(const QString& key, const WaveformData& data);
        
        /// @brief キャッシュされた波形データを取得
        bool getCachedData(const QString& key, WaveformData& data) const;
        
        /// @brief キャッシュをクリア
        void clearCache();
        
        /// @brief キャッシュサイズを取得
        size_t getCacheSize() const;

        // ===== ユーティリティ =====

        /// @brief 推奨サンプルレートを取得（表示幅に基づく）
        static int getRecommendedSamplesPerPixel(int width, float duration);

        /// @brief デフォルト設定を取得
        static WaveformSettings getDefaultSettings();

        // ===== 非同期処理 =====

        /// @brief 非同期で波形画像を生成
        /// @param audioFilePath オーディオファイルパス
        /// @param width 出力幅
        void generateImageAsync(const QString& audioFilePath, int width);
        
        /// @brief 非同期処理をキャンセル
        void cancelAsyncGeneration();

    signals:
        /// @brief 波形生成開始シグナル
        void generationStarted();

        /// @brief 波形生成進捗シグナル
        void generationProgress(float progress);

        /// @brief 波形生成完了シグナル
        void generationCompleted(const WaveformResult& result);

        /// @brief 波形生成失敗シグナル
        void generationFailed(const QString& error);

    private:
        WaveformSettings settings_;
        
        // キャッシュ
        QMap<QString, WaveformData> cache_;
        size_t maxCacheSize_ = 10;  // 最大キャッシュ数
        
        // 内部メソッド
        WaveformData processAudioData(const float* samples, size_t sampleCount, 
                                     int sampleRate, int channels);
        void drawBars(QPainter& painter, const WaveformData& data, int width);
        void drawLines(QPainter& painter, const WaveformData& data, int width);
        void drawFilled(QPainter& painter, const WaveformData& data, int width);
    };

    // ===== ヘルパー関数 =====

    /// @brief WaveformTypeから名称を取得
    export QString waveformTypeToName(WaveformType type);

    /// @brief WaveformStyleから名称を取得
    export QString waveformStyleToName(WaveformStyle style);

    /// @brief 色を16進数文字列に変換
    export QString colorToHex(const QColor& color);

    /// @brief 16進数文字列から色に変換
    export QColor hexToColor(const QString& hex);

} // namespace ArtifactCore
