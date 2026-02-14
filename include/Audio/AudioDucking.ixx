module;

#include <QObject>
#include <QList>
#include <QMap>
#include <QVector>
#include <memory>

export module Audio.Ducking;

import std;

/**
 * @brief Audio Ducking
 * 
 * オーディオダッキング機能
 * メインオーディオ（ナレーション等）が再生されている間に、
 * バックグラウンドオーディオ（BGM等）の音量を自動的に下げます
 */
export namespace ArtifactCore {

    /// @brief ダッキングタイプ
    enum class DuckingType {
        None,           // ダッキングなし
        Compress,       // コンプレッサー方式
        GainReduction,  // ゲインリダクション方式
        Sidechain,      // サイドチェーン方式
    };

    /// @brief プレゼンス検出タイプ
    enum class PresenceDetection {
        Threshold,      // しきい値ベース
        RMS,           // RMSベース
        Peak,          // ピークベース
    };

    /// @brief ダッキング設定
    struct DuckingSettings {
        DuckingType type = DuckingType::GainReduction;
        PresenceDetection detection = PresenceDetection::RMS;
        
        // レベル設定
        float duckLevel = -20.0f;        // ダッキング時の音量（dB）
        float threshold = -40.0f;         // プレゼンス検出しきい値（dB）
        
        // タイミング
        int attackTime = 10;             // アタックタイム（ミリ秒）
        int releaseTime = 300;           // リリースタイム（ミリ秒）
        
        // 検出パラメータ
        int detectionWindow = 50;         // 検出ウィンドウ（ミリ秒）
        float ratio = 0.25f;             // ダッキング比率（0.0 - 1.0）
        
        //  Hold
        int holdTime = 100;               // ホールドタイム（ミリ秒）
        
        // 有効/無効
        bool enabled = true;
    };

    /// @brief オーディオトラック情報
    struct AudioTrackInfo {
        int trackId = 0;
        QString trackName;
        float currentLevel = 0.0f;       // 現在のレベル（dB）
        float currentRMS = 0.0f;         // 現在のRMS
        float currentPeak = 0.0f;        // 現在ピーク
        bool isSilent = true;            // 静音状態
    };

    /// @brief ダッキング結果
    struct DuckingResult {
        float inputGain = 1.0f;          // 入力ゲイン
        float outputGain = 1.0f;         // 出力ゲイン
        float duckingAmount = 0.0f;      // ダッキング量（dB）
        bool isDucking = false;           // ダucking中か
        float targetGain = 1.0f;         // 目標ゲイン
        float currentGain = 1.0f;         // 現在のゲイン
    };

    /// @brief オーディオダッキング管理クラス
    export class AudioDucking : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief コンストラクタ
         * @param parent 親オブジェクト
         */
        explicit AudioDucking(QObject* parent = nullptr);
        
        ~AudioDucking() override;

        // コピー・ムーブ禁止
        AudioDucking(const AudioDucking&) = delete;
        AudioDucking& operator=(const AudioDucking&) = delete;

        // ===== 設定 =====

        /// @brief ダッキングタイプを設定
        void setDuckingType(DuckingType type);
        DuckingType getDuckingType() const { return settings_.type; }

        /// @brief ダッキング設定整体を設定
        void setSettings(const DuckingSettings& settings);
        const DuckingSettings& getSettings() const { return settings_; }

        /// @brief プレゼンス検出タイプを設定
        void setPresenceDetection(PresenceDetection detection);
        PresenceDetection getPresenceDetection() const { return settings_.detection; }

        /// @brief ダッキングレベルを設定（dB）
        void setDuckLevel(float levelDb);
        float getDuckLevel() const { return settings_.duckLevel; }

        /// @brief しきい値を設定（dB）
        void setThreshold(float thresholdDb);
        float getThreshold() const { return settings_.threshold; }

        /// @brief アタックタイムを設定（ミリ秒）
        void setAttackTime(int ms);
        int getAttackTime() const { return settings_.attackTime; }

        /// @brief リリースタイムを設定（ミリ秒）
        void setReleaseTime(int ms);
        int getReleaseTime() const { return settings_.releaseTime; }

        /// @brief 有効/無効を設定
        void setEnabled(bool enabled);
        bool isEnabled() const { return settings_.enabled; }

        // ===== トラック管理 =====

        /// @brief トラックを追加
        void addTrack(int trackId, const QString& name);
        
        /// @brief トラックを削除
        void removeTrack(int trackId);

        /// @brief 全てのトラックをクリア
        void clearTracks();

        /// @brief メインソーストラックを設定
        void setMainSourceTrack(int trackId);
        int getMainSourceTrack() const { return mainSourceTrackId_; }

        /// @brief ダッキング対象トラックを設定
        void addDuckingTarget(int trackId);
        
        /// @brief ダッキング対象トラックを削除
        void removeDuckingTarget(int trackId);

        // ===== プロセス =====

        /// @brief サンプルを処理（単一トラック）
        /// @param trackId トラックID
        /// @param samples サンプルデータ
        /// @param sampleCount サンプル数
        /// @return ダッキング結果
        DuckingResult processTrack(int trackId, const float* samples, size_t sampleCount);

        /// @brief サンプルを処理（複数トラック）
        /// @param trackLevels 各トラックのレベル情報
        /// @return 各トラックのダッキング結果
        QMap<int, DuckingResult> processAllTracks(const QMap<int, AudioTrackInfo>& trackLevels);

        /// @brief ゲインを計算（マスターチャンク用）
        /// @param mainLevel メインソースのレベル
        /// @param backgroundLevel 背景音频のレベル
        /// @return 適用するゲイン
        float calculateGain(float mainLevel, float backgroundLevel);

        // ===== ユーティリティ =====

        /// @brief レベルをdBに変換
        static float linearToDb(float linear);

        /// @brief リニアに変換
        static float dbToLinear(float db);

        /// @brief RMSを計算
        static float calculateRMS(const float* samples, size_t count);

        /// @brief ピークを計算
        static float calculatePeak(const float* samples, size_t count);

        /// @brief 推奨ダッキング設定をを取得
        static DuckingSettings getRecommendedSettings();

        // ===== 状態 =====

        /// @brief 現在ダucking中か
        bool isCurrentlyDucking() const { return isDucking_; }

        /// @brief 現在のダッキング量を取得
        float getCurrentDuckAmount() const { return currentDuckAmount_; }

    signals:
        /// @brief ダッキング開始シグナル
        void duckingStarted(int trackId);

        /// @brief ダッキング終了シグナル
        void duckingEnded(int trackId);

        /// @brief ダッキング状態変化シグナル
        void duckingStateChanged(bool isDucking, float amount);

        /// @brief レベル変化シグナル
        void levelChanged(int trackId, float level);

    private:
        DuckingSettings settings_;
        
        // トラック管理
        QMap<int, QString> tracks_;
        QSet<int> duckingTargets_;
        int mainSourceTrackId_ = -1;
        
        // 状態
        bool isDucking_ = false;
        float currentDuckAmount_ = 0.0f;
        
        // エンベロープ
        float envelope_ = 0.0f;
        qint64 lastAttackTime_ = 0;
        
        // 内部メソッド
        float detectPresence(const float* samples, size_t count);
        float applyEnvelope(float target, float current, bool attacking);
        bool checkThreshold(float level);
    };

    // ===== ヘルパー関数 =====

    /// @brief DuckingTypeから名称を取得
    export QString duckingTypeToName(DuckingType type);

    /// @brief PresenceDetectionから名称を取得
    export QString detectionTypeToName(PresenceDetection detection);

} // namespace ArtifactCore
