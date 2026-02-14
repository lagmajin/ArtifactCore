module;

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>

export module Asset.ProxyCacheManager;

import Asset.ProxyCacheManager;

namespace ArtifactCore {

    // Impl クラス宣言
    class ProxyCacheManager::Impl {
    public:
        // RAMキャッシュ: key = originalPath, value = cache entry
        QCache<QString, ProxyCacheEntry> ramCache_;
        
        // ディスクキャッシュのインデックス: key = originalPath
        QMap<QString, ProxyCacheEntry> diskCacheIndex_;
        
        // アクセス統計
        QMap<QString, int> accessCounts_;
        QMap<QString, qint64> accessTimes_;
        
        // 統計
        int64_t currentRAMCacheSize_ = 0;
        int64_t currentDiskCacheSize_ = 0;
        int hitCount_ = 0;
        int missCount_ = 0;
    };

    // ProxyCacheManager コンストラクタ
    ProxyCacheManager::ProxyCacheManager(QObject* parent)
        : QObject(parent)
        , impl_(std::make_unique<Impl>())
    {
        // デフォルトキャッシュディレクトリ
        cacheDirectory_ = QDir::cleanPath(QDir::homePath() + "/.artifactcore/proxy_cache");
        
        // RAMキャッシュのコスト計算をエントリサイズに設定
        impl_->ramCache_.setMaxCost(maxRAMCacheSize_);
    }

    // デストラクタ
    ProxyCacheManager::~ProxyCacheManager() = default;

    // RAMキャッシュ最大サイズを設定
    void ProxyCacheManager::setMaxRAMCacheSize(int64_t bytes)
    {
        maxRAMCacheSize_ = bytes;
        impl_->ramCache_.setMaxCost(bytes);
    }

    // ディスクキャッシュ最大サイズを設定
    void ProxyCacheManager::setMaxDiskCacheSize(int64_t bytes)
    {
        maxDiskCacheSize_ = bytes;
        
        // 自動クリーンアップが必要かチェック
        if (autoCleanupEnabled_ && impl_->currentDiskCacheSize_ > bytes) {
            QTimer::singleShot(0, this, &ProxyCacheManager::performAutoCleanup);
        }
    }

    // キャッシュポリシーを設定
    void ProxyCacheManager::setCachePolicy(CachePolicy policy)
    {
        cachePolicy_ = policy;
    }

    // キャッシュディレクトリを設定
    void ProxyCacheManager::setCacheDirectory(const QString& directory)
    {
        cacheDirectory_ = directory;
    }

    // 自動クリーンアップを有効にするか設定
    void ProxyCacheManager::setAutoCleanupEnabled(bool enabled)
    {
        autoCleanupEnabled_ = enabled;
    }

    // プロキシファイルがキャッシュされているか確認
    bool ProxyCacheManager::isCached(const QString& originalPath) const
    {
        QString key = generateCacheKey(originalPath);
        
        // RAMキャッシュをチェック
        if (impl_->ramCache_.contains(key)) {
            return true;
        }
        
        // ディスクキャッシュインデックスをチェック
        return impl_->diskCacheIndex_.contains(key);
    }

    // プロキシキャッシュ状態を取得
    CacheState ProxyCacheManager::getCacheState(const QString& originalPath) const
    {
        QString key = generateCacheKey(originalPath);
        
        if (impl_->ramCache_.contains(key)) {
            return CacheState::Available;
        }
        
        if (impl_->diskCacheIndex_.contains(key)) {
            auto& entry = impl_->diskCacheIndex_[key];
            if (entry.isValid && QFileInfo::exists(entry.proxyPath)) {
                return CacheState::Available;
            }
        }
        
        return CacheState::NotCached;
    }

    // プロキンパスを取得
    QString ProxyCacheManager::getProxyPath(const QString& originalPath) const
    {
        QString key = generateCacheKey(originalPath);
        
        // RAMキャッシュを優先
        if (impl_->ramCache_.contains(key)) {
            return impl_->ramCache_[key]->proxyPath;
        }
        
        // ディスクキャッシュ
        if (impl_->diskCacheIndex_.contains(key)) {
            return impl_->diskCacheIndex_[key].proxyPath;
        }
        
        return QString();
    }

    // キャッシュエントリを追加
    void ProxyCacheManager::addCacheEntry(const QString& originalPath, const ProxyCacheEntry& entry)
    {
        QString key = generateCacheKey(originalPath);
        
        // ディスクキャッシュインデックスに追加
        impl_->diskCacheIndex_[key] = entry;
        impl_->currentDiskCacheSize_ += entry.fileSize;
        
        // 自動クリーンアップが必要かチェック
        if (autoCleanupEnabled_ && impl_->currentDiskCacheSize_ > maxDiskCacheSize_) {
            QTimer::singleShot(0, this, &ProxyCacheManager::performAutoCleanup);
        }
        
        emit cacheStateChanged(originalPath, CacheState::Available);
    }

    // キャッシュエントリを削除
    void ProxyCacheManager::removeCacheEntry(const QString& originalPath)
    {
        QString key = generateCacheKey(originalPath);
        
        // RAMキャッシュから削除
        impl_->ramCache_.remove(key);
        
        // ディスクキャッシュから削除
        if (impl_->diskCacheIndex_.contains(key)) {
            impl_->currentDiskCacheSize_ -= impl_->diskCacheIndex_[key].fileSize;
            impl_->diskCacheIndex_.remove(key);
        }
        
        // 統計を更新
        impl_->accessCounts_.remove(key);
        impl_->accessTimes_.remove(key);
        
        emit cacheStateChanged(originalPath, CacheState::NotCached);
    }

    // すべてのキャッシュをクリア
    void ProxyCacheManager::clearCache()
    {
        clearRAMCache();
        clearDiskCache();
    }

    // RAMキャッシュをクリア
    void ProxyCacheManager::clearRAMCache()
    {
        impl_->ramCache_.clear();
        impl_->currentRAMCacheSize_ = 0;
    }

    // ディスクキャッシュをクリア
    void ProxyCacheManager::clearDiskCache()
    {
        // ディスク上のファイルを削除
        QDir cacheDir(cacheDirectory_);
        if (cacheDir.exists()) {
            cacheDir.removeRecursively();
        }
        
        impl_->diskCacheIndex_.clear();
        impl_->currentDiskCacheSize_ = 0;
        
        // データベースもクリア
        QString dbPath = cacheDirectory_ + "/cache_database.json";
        QFile::remove(dbPath);
    }

    // エントリを無効化
    void ProxyCacheManager::invalidateEntry(const QString& originalPath)
    {
        QString key = generateCacheKey(originalPath);
        
        if (impl_->ramCache_.contains(key)) {
            impl_->ramCache_[key]->isValid = false;
        }
        
        if (impl_->diskCacheIndex_.contains(key)) {
            impl_->diskCacheIndex_[key].isValid = false;
        }
        
        emit cacheStateChanged(originalPath, CacheState::NotCached);
    }

    // アクセスを記録
    void ProxyCacheManager::recordAccess(const QString& originalPath)
    {
        QString key = generateCacheKey(originalPath);
        
        impl_->accessTimes_[key] = QDateTime::currentMSecsSinceEpoch();
        impl_->accessCounts_[key] = impl_->accessCounts_.value(key, 0) + 1;
        
        impl_->hitCount_++;
    }

    // ディスククリーンアップを実行
    void ProxyCacheManager::cleanupDiskCache()
    {
        evictDiskCache(maxDiskCacheSize_ * 0.9); // 90%になるまで削除
    }

    // RAMクリーンアップを実行
    void ProxyCacheManager::cleanupRAMCache()
    {
        evictRAMCache(maxRAMCacheSize_ * 0.9);
    }

    // 自動クリーンアップを実行
    void ProxyCacheManager::performAutoCleanup()
    {
        int64_t freedBytes = 0;
        
        // ディスククリーンアップ
        if (impl_->currentDiskCacheSize_ > maxDiskCacheSize_) {
            freedBytes += impl_->currentDiskCacheSize_ - maxDiskCacheSize_;
            cleanupDiskCache();
        }
        
        // RAMクリーンアップ
        if (impl_->currentRAMCacheSize_ > maxRAMCacheSize_) {
            freedBytes += impl_->currentRAMCacheSize_ - maxRAMCacheSize_;
            cleanupRAMCache();
        }
        
        emit cleanupCompleted(freedBytes);
    }

    // 古いキャッシュをクリーンアップ
    void ProxyCacheManager::cleanupOldCache(int daysToKeep)
    {
        QDateTime threshold = QDateTime::currentDateTime().addDays(-daysToKeep);
        int64_t freedBytes = 0;
        
        auto it = impl_->diskCacheIndex_.begin();
        while (it != impl_->diskCacheIndex_.end()) {
            if (QDateTime::fromMSecsSinceEpoch(it.value().createdTime) < threshold) {
                freedBytes += it.value().fileSize;
                QFile::remove(it.value().proxyPath);
                it = impl_->diskCacheIndex_.erase(it);
            } else {
                ++it;
            }
        }
        
        impl_->currentDiskCacheSize_ -= freedBytes;
        emit cleanupCompleted(freedBytes);
    }

    // 統計を取得
    ProxyCacheStats ProxyCacheManager::getStatistics() const
    {
        ProxyCacheStats stats;
        stats.totalRAMCacheSize = impl_->currentRAMCacheSize_;
        stats.maxRAMCacheSize = maxRAMCacheSize_;
        stats.diskCacheSize = impl_->currentDiskCacheSize_;
        stats.maxDiskCacheSize = maxDiskCacheSize_;
        stats.ramCacheCount = impl_->ramCache_.count();
        stats.diskCacheCount = impl_->diskCacheIndex_.count();
        stats.hitCount = impl_->hitCount_;
        stats.missCount = impl_->missCount_;
        
        int total = stats.hitCount + stats.missCount;
        stats.hitRate = total > 0 ? (double)stats.hitCount / total : 0.0;
        
        return stats;
    }

    // ディスクキャッシュサイズを再計算
    void ProxyCacheManager::recalculateDiskCacheSize()
    {
        impl_->currentDiskCacheSize_ = 0;
        
        for (auto& entry : impl_->diskCacheIndex_) {
            if (QFileInfo::exists(entry.proxyPath)) {
                impl_->currentDiskCacheSize_ += entry.fileSize;
            }
        }
    }

    // キャッシュディレクトリを初期化
    void ProxyCacheManager::initializeCacheDirectory()
    {
        QDir dir(cacheDirectory_);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    }

    // キャッシュディレクトリをスキャン
    void ProxyCacheManager::scanCacheDirectory()
    {
        QDir dir(cacheDirectory_);
        if (!dir.exists()) {
            return;
        }
        
        QStringList filters;
        filters << "*.mp4" << "*.mov" << "*.webm" << "*.avi" << "*.mxf";
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        
        for (const QFileInfo& fileInfo : files) {
            ProxyCacheEntry entry;
            entry.proxyPath = fileInfo.absoluteFilePath();
            entry.fileSize = fileInfo.size();
            entry.createdTime = fileInfo.created().toMSecsSinceEpoch();
            entry.lastAccessTime = fileInfo.lastRead().toMSecsSinceEpoch();
            entry.isValid = true;
            
            // 元ファイルのパスを推定（サフィックスを削除）
            QString baseName = fileInfo.baseName();
            // ここで元ファイルへのマッピングが必要（将来的にはDBが必要）
            
            QString key = generateCacheKey(entry.proxyPath);
            impl_->diskCacheIndex_[key] = entry;
        }
        
        recalculateDiskCacheSize();
    }

    // キャッシュデータベースを保存
    void ProxyCacheManager::saveCacheDatabase()
    {
        QJsonArray array;
        
        for (auto it = impl_->diskCacheIndex_.constBegin(); it != impl_->diskCacheIndex_.constEnd(); ++it) {
            QJsonObject obj;
            obj["key"] = it.key();
            obj["proxyPath"] = it.value().proxyPath;
            obj["originalPath"] = it.value().originalPath;
            obj["fileSize"] = it.value().fileSize;
            obj["createdTime"] = it.value().createdTime;
            obj["lastAccessTime"] = it.value().lastAccessTime;
            obj["isValid"] = it.value().isValid;
            array.append(obj);
        }
        
        QJsonDocument doc(array);
        QString dbPath = cacheDirectory_ + "/cache_database.json";
        QFile file(dbPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        }
    }

    // キャッシュデータベースを読込
    void ProxyCacheManager::loadCacheDatabase()
    {
        QString dbPath = cacheDirectory_ + "/cache_database.json";
        QFile file(dbPath);
        
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            return;
        }
        
        QJsonArray array = doc.array();
        
        for (const QJsonValue& value : array) {
            QJsonObject obj = value.toObject();
            
            ProxyCacheEntry entry;
            entry.proxyPath = obj["proxyPath"].toString();
            entry.originalPath = obj["originalPath"].toString();
            entry.fileSize = obj["fileSize"].toInt64();
            entry.createdTime = obj["createdTime"].toInteger();
            entry.lastAccessTime = obj["lastAccessTime"].toInteger();
            entry.isValid = obj["isValid"].toBool();
            
            // ファイルが存在するか確認
            if (entry.isValid && QFileInfo::exists(entry.proxyPath)) {
                impl_->diskCacheIndex_[obj["key"].toString()] = entry;
            }
        }
        
        recalculateDiskCacheSize();
    }

    // RAMキャッシュからエビクション
    void ProxyCacheManager::evictRAMCache(int64_t bytesNeeded)
    {
        // 現在使用中のポリシーに基づいて削除
        switch (cachePolicy_) {
        case CachePolicy::LRU:
        case CachePolicy::FIFO:
            // QCacheが自動的にLRUを管理
            break;
            
        case CachePolicy::LFU:
        case CachePolicy::Size:
            // カスタムロジックが必要
            break;
        }
        
        // 現在のRAMサイズを目標値以下に削減
        while (impl_->currentRAMCacheSize_ > bytesNeeded && !impl_->ramCache_.isEmpty()) {
            // QCacheから古いエントリを削除
            // （QCacheが内部でLRUを管理しているため、特に何もせずとも自動削除）
            impl_->currentRAMCacheSize_ = impl_->ramCache_.totalCost();
            break;
        }
    }

    // ディスクキャッシュからエビクション
    void ProxyCacheManager::evictDiskCache(int64_t bytesNeeded)
    {
        // アクセス時間に基づいて古いエントリを削除
        QList<QString> keys = impl_->diskCacheIndex_.keys();
        
        // アクセス時間でソート
        std::sort(keys.begin(), keys.end(), [this](const QString& a, const QString& b) {
            return impl_->accessTimes_.value(a, 0) < impl_->accessTimes_.value(b, 0);
        });
        
        int64_t freedBytes = 0;
        
        for (const QString& key : keys) {
            if (impl_->currentDiskCacheSize_ - freedBytes <= bytesNeeded) {
                break;
            }
            
            auto& entry = impl_->diskCacheIndex_[key];
            freedBytes += entry.fileSize;
            
            // ファイルを削除
            QFile::remove(entry.proxyPath);
            
            // インデックスから削除
            impl_->diskCacheIndex_.remove(key);
        }
        
        impl_->currentDiskCacheSize_ -= freedBytes;
    }

    // キャッシュキーを生成
    QString ProxyCacheManager::generateCacheKey(const QString& originalPath) const
    {
        // 絶対パスを正規化してキーとして使用
        return QFileInfo(originalPath).absoluteFilePath();
    }

    // キャッシュサイズをフォーマット
    QString formatCacheSize(int64_t bytes)
    {
        const int64_t KB = 1024;
        const int64_t MB = KB * 1024;
        const int64_t GB = MB * 1024;
        
        if (bytes >= GB) {
            return QString::number(bytes / (double)GB, 'f', 2) + " GB";
        } else if (bytes >= MB) {
            return QString::number(bytes / (double)MB, 'f', 2) + " MB";
        } else if (bytes >= KB) {
            return QString::number(bytes / (double)KB, 'f', 2) + " KB";
        } else {
            return QString::number(bytes) + " B";
        }
    }

    // ヒット率をフォーマット
    QString formatHitRate(double hitRate)
    {
        return QString::number(hitRate * 100, 'f', 2) + "%";
    }

} // namespace ArtifactCore
