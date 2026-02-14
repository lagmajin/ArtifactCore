module;

#include <QString>
#include <QObject>
#include <QSize>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QVariant>

export module Asset.ProxyGenerator;

import std;

/**
 * @brief Proxy Generator
 * 
 * FFmpegを使用して元動画からプロキシ動画を生成するクラス
 * バックグラウンドでの非同期処理に対応
 */
export namespace ArtifactCore {

    /// @brief 生成タスクの状態
    enum class ProxyTaskState {
        Pending,        // 待機中
        Processing,    // 処理中
        Completed,      // 完了
        Failed,        // 失敗
        Cancelled,     // キャンセル
    };

    /// @brief 生成タスク情報
    struct ProxyTask {
        QString id;                     // タスクID
        QString sourcePath;             // 元ファイルパス
        QString proxyPath;              // プロキシ出力パス
        QSize targetSize;               // ターゲット解像度
        int targetBitrate = 0;          // ターゲットビットレート（kbps）
        int targetFramerate = 0;       // ターゲットフレームレート
        ProxyTaskState state = ProxyTaskState::Pending;
        float progress = 0.0f;          // 進捗（0.0 - 1.0）
        QString errorMessage;           // エラーメッセージ
        qint64 startTime = 0;          // 開始時刻
        qint64 endTime = 0;            // 終了時刻
        
        // 設定
        bool useHardwareAcceleration = true;
        int quality = 2;               // 品質（0-5, 0が最高品質）
    };

    /// @brief 進捗コールバックタイプ
    using ProgressCallback = std::function<void(float progress, const QString& message)>;
    using CompletionCallback = std::function<void(bool success, const QString& error)>;

    /// @brief プロキシ動画生成クラス
    export class ProxyGenerator : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief コンストラクタ
         * @param parent 親オブジェクト
         */
        explicit ProxyGenerator(QObject* parent = nullptr);
        
        ~ProxyGenerator() override;

        // コピー・ムーブ禁止
        ProxyGenerator(const ProxyGenerator&) = delete;
        ProxyGenerator& operator=(const ProxyGenerator&) = delete;
        ProxyGenerator(ProxyGenerator&&) = delete;
        ProxyGenerator& operator=(ProxyGenerator&&) = delete;

        // ===== 設定 =====

        /// @brief FFmpegのパスを設定
        void setFFmpegPath(const QString& path);
        QString getFFmpegPath() const { return ffmpegPath_; }

        /// @brief 一時ディレクトリを設定
        void setTempDirectory(const QString& path);
        QString getTempDirectory() const { return tempDirectory_; }

        /// @brief ハードウェアエンコードを有効にするか
        void setHardwareAccelerationEnabled(bool enabled);
        bool isHardwareAccelerationEnabled() const { return hardwareAcceleration_; }

        /// @brief 同時実行タスク数を設定
        void setMaxConcurrentTasks(int count);
        int getMaxConcurrentTasks() const { return maxConcurrentTasks_; }

        /// @brief 優先度を設定
        void setPriority(QThread::Priority priority);
        QThread::Priority getPriority() const { return priority_; }

        // ===== 生成タスク =====

        /// @brief プロキシ動画を生成（同期）
        /// @param sourcePath 元ファイルパス
        /// @param settings プロキシ設定
        /// @return 生成成否
        bool generateSync(const QString& sourcePath, const class ProxySettings& settings);

        /// @brief プロキシ動画を生成（同期、カスタム設定）
        bool generateSync(const QString& sourcePath,
                         const QString& outputPath,
                         const QSize& targetSize,
                         int bitrate = 0,
                         int framerate = 0);

        /// @brief プロキシ動画を生成（非同期）
        /// @return タスクID
        QString generateAsync(const QString& sourcePath, const class ProxySettings& settings);

        /// @brief プロキシ動画を生成（非同期、カスタム設定）
        QString generateAsync(const QString& sourcePath,
                             const QString& outputPath,
                             const QSize& targetSize,
                             int bitrate = 0,
                             int framerate = 0);

        /// @brief タスクをキャンセル
        void cancelTask(const QString& taskId);

        /// @brief すべてのタスクをキャンセル
        void cancelAllTasks();

        /// @brief タスクを削除
        void removeTask(const QString& taskId);

        // ===== タスク情報 =====

        /// @brief タスク一覧を取得
        QList<ProxyTask> getAllTasks() const;

        /// @brief タスクを取得
        ProxyTask getTask(const QString& taskId) const;

        /// @brief タスクの状態を取得
        ProxyTaskState getTaskState(const QString& taskId) const;

        /// @brief タスクの進捗を取得
        float getTaskProgress(const QString& taskId) const;

        // ===== ユーティリティ =====

        /// @brief 元動画情報を取得
        struct VideoInfo {
            int width = 0;
            int height = 0;
            double frameRate = 0.0;
            int64_t duration = 0;     // ミリ秒
            int64_t fileSize = 0;      // バイト
            QString codec;
            int bitrate = 0;
            int streamCount = 0;
            bool hasVideo = false;
            bool hasAudio = false;
        };

        /// @brief 動画情報を取得
        VideoInfo getVideoInfo(const QString& videoPath) const;

        /// @brief FFmpegが利用可能か確認
        bool isFFmpegAvailable() const;

        /// @brief 推奨ビットレートを計算（解像度に基づく）
        int calculateRecommendedBitrate(int width, int height) const;

    signals:
        /// @brief タスク開始シグナル
        void taskStarted(const QString& taskId);

        /// @brief タスク進捗シグナル
        void taskProgress(const QString& taskId, float progress, const QString& message);

        /// @brief タスク完了シグナル
        void taskCompleted(const QString& taskId, bool success, const QString& error);

        /// @brief 全タスク完了シグナル
        void allTasksCompleted();

        /// @brief キュー変更シグナル
        void queueChanged(int pendingCount, int processingCount);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
        
        // 設定
        QString ffmpegPath_;
        QString tempDirectory_;
        bool hardwareAcceleration_ = true;
        int maxConcurrentTasks_ = 2;
        QThread::Priority priority_ = QThread::NormalPriority;

        // 内部メソッド
        QString generateTaskId();
        QString buildFFmpegArgs(const ProxyTask& task) const;
        bool executeFFmpeg(const QStringList& args, const QString& taskId,
                          std::function<void(float)> progressCallback);
        void processTaskQueue();
    };

    // ===== ヘルパー関数 =====

    /// @brief 解像度から名称を取得
    export QString resolutionToName(int width, int height);

    /// @brief ビットレートから品質レベルを取得
    export QString bitrateToQualityLabel(int bitrate);

} // namespace ArtifactCore
