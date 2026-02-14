module;

#include <QString>
#include <QSize>
#include <QDir>
#include <QObject>
#include <QMutex>
#include <QCache>
#include <QSet>

export module Asset.ProxyCacheManager;

import std;

/**
 * @brief Proxy Cache Manager
 * 
 * プロキシファイルのキャッシュ管理クラス
 * RAMキャッシュとディスクキャッシュを管理し、効率的なプロキシアクセスを提供
 */
export namespace ArtifactCore {

    /// @brief キャッシュエントリ情報
    struct ProxyCacheEntry {
        QString proxyPath;          // プロキシファイルパス
        QString originalPath;       // 元ファイルパス
        QSize originalSize;        // 元ファイルのサイズ
        QSize proxySize;           // プロキシのサイズ
        int64_t fileSize = 0;      // プロキシファイルサイズ（バイト）
        qint64 lastAccessTime = 0; // 最終アクセス時刻
        qint64 createdTime = 0;    // 作成時刻
        bool isValid = false;      // エントリ有効フラグ
    };

    /// @brief キャッシュ統計
    struct ProxyCacheStats {
        int64_t totalRAMCacheSize = 0;      // RAMキャッシュサイズ（バイト）
        int64_t maxRAMCacheSize = 0;        // 最大RAMキャッシュサイズ
        int64_t diskCacheSize = 0;          // ディスクキャッシュサイズ
        int64_t maxDiskCacheSize = 0;       // 最大ディスクキャッシュサイズ
        int ramCacheCount = 0;              // RAMキャッシュエントリ数
        int diskCacheCount = 0;             // ディスクキャッシュエントリ数
        int hitCount = 0;                   // キャッシュヒット数
        int missCount = 0;                  // キャッシュミス数
        double hitRate = 0.0;               // ヒット率
    };

    /// @brief キャッシュポリシー
    enum class CachePolicy {
        LRU,        // Least Recently Used
        LFU,        // Least Frequently Used
        FIFO,       // First In First Out
        Size,       // サイズ順（小さいものから削除）
    };

    /// @brief キャッシュ状態
    enum class CacheState {
        Available,      // キャッシュ利用可能
        Generating,     // 生成中
        NotCached,      // キャッシュなし
        Error,          // エラー
    };

    /// @brief プロキシキャッシュ管理クラス
    export class ProxyCacheManager : public QObject {
        Q_OBJECT

    public:
        /**
         * @brief コンストラクタ
         * @param parent 親オブジェクト
         */
        explicit ProxyCacheManager(QObject* parent = nullptr);
        
        ~ProxyCacheManager() override;

        // コピー・ムーブ禁止
        ProxyCacheManager(const ProxyCacheManager&) = delete;
        ProxyCacheManager& operator=(const ProxyCacheManager&) = delete;
        ProxyCacheManager(ProxyCacheManager&&) = delete;
        ProxyCacheManager& operator=(ProxyCacheManager&&) = delete;

        // ===== キャッシュ設定 =====

        /// @brief RAMキャッシュ最大サイズを設定（バイト）
        void setMaxRAMCacheSize(int64_t bytes);
        int64_t getMaxRAMCacheSize() const { return maxRAMCacheSize_; }

        /// @brief ディスクキャッシュ最大サイズを設定（バイト）
        void setMaxDiskCacheSize(int64_t bytes);
        int64_t getMaxDiskCacheSize() const { return maxDiskCacheSize_; }

        /// @brief キャッシュポリシーを設定
        void setCachePolicy(CachePolicy policy);
        CachePolicy getCachePolicy() const { return cachePolicy_; }

        /// @brief キャッシュディレクトリを設定
        void setCacheDirectory(const QString& directory);
        QString getCacheDirectory() const { return cacheDirectory_; }

        /// @brief 自動クリーンアップを有効にするか
        void setAutoCleanupEnabled(bool enabled);
        bool isAutoCleanupEnabled() const { return autoCleanupEnabled_; }

        // ===== キャッシュ操作 =====

        /// @brief プロキシファイルがキャッシュされているか確認
        bool isCached(const QString& originalPath) const;

        /// @brief プロキシキャッシュ状態を取得
        CacheState getCacheState(const QString& originalPath) const;

        /// @brief プロキンパスを取得（キャッシュされていれば）
        QString getProxyPath(const QString& originalPath) const;

        /// @brief キャッシュエントリを追加
        void addCacheEntry(const QString& originalPath, const ProxyCacheEntry& entry);

        /// @brief キャッシュエントリを削除
        void removeCacheEntry(const QString& originalPath);

        /// @brief すべてのキャッシュをクリア
        void clearCache();

        /// @brief RAMキャッシュをクリア
        void clearRAMCache();

        /// @brief ディスクキャッシュをクリア
        void clearDiskCache();

        /// @brief 指定した元のファイルエントリを無効化
        void invalidateEntry(const QString& originalPath);

        /// @brief エントリへのアクセスを記録
        void recordAccess(const QString& originalPath);

        // ===== クリーンアップ =====

        /// @brief ディスククリーンアップを実行
        void cleanupDiskCache();

        /// @brief RAMクリーンアップを実行
        void cleanupRAMCache();

        /// @brief 自動クリーンアップを実行
        void performAutoCleanup();

        /// @brief 古いキャッシュをクリーンアップ（日数指定）
        void cleanupOldCache(int daysToKeep);

        // ===== 統計情報 =====

        /// @brief キャッシュ統計を取得
        ProxyCacheStats getStatistics() const;

        /// @brief ディスクキャッシュサイズを再計算
        void recalculateDiskCacheSize();

        // ===== ユーティリティ =====

        /// @brief キャッシュディレクトリを初期化
        void initializeCacheDirectory();

        /// @brief キャッシュディレクトリをスキャンしてインデックスを構築
        void scanCacheDirectory();

        /// @brief データベースファイルにキャッシュ情報を保存
        void saveCacheDatabase();

        /// @brief データベースファイルからキャッシュ情報を読込
        void loadCacheDatabase();

    signals:
        /// @brief キャッシュ状態変化 сигнал
        void cacheStateChanged(const QString& originalPath, CacheState newState);

        /// @brief クリーンアップ完了 сигнал
        void cleanupCompleted(int64_t bytesFreed);

        /// @brief 統計更新 сигнал
        void statisticsUpdated(const ProxyCacheStats& stats);

        /// @brief プロキシ生成開始 сигнал
        void proxyGenerationStarted(const QString& originalPath);

        /// @brief プロキシ生成完了 сигнал
        void proxyGenerationCompleted(const QString& originalPath, bool success);

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
        
        // 設定
        int64_t maxRAMCacheSize_ = 1024 * 1024 * 1024;  // 1GB
        int64_t maxDiskCacheSize_ = 10LL * 1024 * 1024 * 1024;  // 10GB
        CachePolicy cachePolicy_ = CachePolicy::LRU;
        QString cacheDirectory_;
        bool autoCleanupEnabled_ = true;

        // 内部メソッド
        void evictRAMCache(int64_t bytesNeeded);
        void evictDiskCache(int64_t bytesNeeded);
        QString generateCacheKey(const QString& originalPath) const;
    };

    // ===== ヘルパー関数 =====

    /// @brief キャッシュサイズのフォーマット（、人間-readableな文字列）
    export QString formatCacheSize(int64_t bytes);

    /// @brief ヒット率のフォーマット
    export QString formatHitRate(double hitRate);

} // namespace ArtifactCore
