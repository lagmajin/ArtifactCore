module;
#include <utility>

#include <memory>
#include "../Define/DllExportMacro.hpp"
#include <QMap>
#include <QReadWriteLock>

export module Audio.Cache;

import Audio.Segment;

export namespace ArtifactCore
{

// オーディオキャッシュエントリ
struct CachedAudioFrame {
    int64_t frameNumber = 0;
    AudioSegment pcm;
    qint64 lastAccess = 0;  // LRU 用タイムスタンプ
    
    CachedAudioFrame() = default;
    CachedAudioFrame(int64_t frame, AudioSegment&& data, qint64 accessTime)
        : frameNumber(frame), pcm(std::move(data)), lastAccess(accessTime) {}
};

// オーディオデコード結果のキャッシュ管理
export class LIBRARY_DLL_API AudioCache
{
public:
    AudioCache();
    ~AudioCache() = default;
    
    // キャッシュから取得
    bool getCached(int64_t frameNumber, AudioSegment& out);
    
    // キャッシュに追加
    void addCache(int64_t frameNumber, AudioSegment&& pcm);
    
    // プリフェッチ（バックグラウンドでキャッシュ）
    void prefetch(int64_t startFrame, int frameCount);
    
    // 期限切れエントリのクリア
    void clearExpired(qint64 maxAgeMs = 30000);  // 30秒以上アクセスなし
    
    // 統計情報
    size_t getCacheSize() const;
    size_t getMemoryUsage() const;  // バイト単位
    
    // 設定
    void setMaxCacheFrames(int maxFrames) { maxCacheFrames_ = maxFrames; }
    int getMaxCacheFrames() const { return maxCacheFrames_; }
    
    // クリア
    void clear();

private:
    QMap<int64_t, CachedAudioFrame> cache_;
    mutable QReadWriteLock lock_;
    int maxCacheFrames_ = 300;  // デフォルト10秒分 (30fps)
    qint64 lastCleanupTime_ = 0;
    
    // LRU クリーンアップ
    void cleanupLRU();
};

} // namespace ArtifactCore
