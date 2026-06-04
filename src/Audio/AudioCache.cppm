module;
class tst_QList;
#include <QDateTime>
#include <QDebug>
#include <QReadWriteLock>
#include <QVector>

module Audio.Cache;

import std;

namespace ArtifactCore
{

AudioCache::AudioCache()
{
    qDebug() << "[AudioCache] Created with max frames:" << maxCacheFrames_;
}

bool AudioCache::getCached(int64_t frameNumber, AudioSegment& out)
{
    QReadLocker locker(&lock_);
    
    auto it = cache_.find(frameNumber);
    if (it != cache_.end()) {
        it->lastAccess = QDateTime::currentMSecsSinceEpoch();
        out = it->pcm;  // コピー
        return true;
    }
    
    return false;
}

void AudioCache::addCache(int64_t frameNumber, AudioSegment&& pcm)
{
    QWriteLocker locker(&lock_);
    
    // 既存エントリがある場合は置き換え
    auto it = cache_.find(frameNumber);
    if (it != cache_.end()) {
        it->pcm = std::move(pcm);
        it->lastAccess = QDateTime::currentMSecsSinceEpoch();
        return;
    }
    
    // 新規エントリ
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    cache_.insert(frameNumber, CachedAudioFrame(frameNumber, std::move(pcm), now));
    
    // キャッシュサイズ超過時はクリーンアップ
    if (cache_.size() > maxCacheFrames_) {
        cleanupLRU();
    }
}

void AudioCache::prefetch(int64_t startFrame, int frameCount)
{
    // プリフェッチは外部から呼び出される（デコード側で実装）
    // ここではログ出力のみ
    qDebug() << "[AudioCache] Prefetch requested:" << startFrame << "to" << (startFrame + frameCount - 1);
}

void AudioCache::clearExpired(qint64 maxAgeMs)
{
    QWriteLocker locker(&lock_);
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 cutoffTime = now - maxAgeMs;
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (it->lastAccess < cutoffTime) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    qDebug() << "[AudioCache] Cleared expired entries, remaining:" << cache_.size();
}

size_t AudioCache::getCacheSize() const
{
    QReadLocker locker(&lock_);
    return cache_.size();
}

size_t AudioCache::getMemoryUsage() const
{
    QReadLocker locker(&lock_);

    size_t totalBytes = 0;
    for (const auto& entry : cache_) {
        // AudioSegment のメモリ使用量を概算
        totalBytes += entry.pcm.frameCount() * entry.pcm.channelCount() * sizeof(float);
    }

    return totalBytes;
}

void AudioCache::clear()
{
    QWriteLocker locker(&lock_);
    cache_.clear();
    qDebug() << "[AudioCache] Cache cleared";
}

    // PERF: std::sort (O(n log n)) の代わりに std::nth_element (O(n)) を使用。
    // 中央値を見つけて古い半分を削除する。ソート不要で LRU 分割が可能。
    // 参照: docs/AUDIO_PERFORMANCE_ARCHITECTURE_2026-06-05.md
    void AudioCache::cleanupLRU()
{
    // LRU (Least Recently Used) 方式で古いエントリを削除
    if (cache_.isEmpty()) return;
    
    // lastAccess の中央値を見つけて、古い半分を削除（O(n) nth_element 使用）
    QVector<CachedAudioFrame*> entries;
    entries.reserve(cache_.size());
    
    for (auto& entry : cache_) {
        entries.append(&entry);
    }
    
    const int mid = entries.size() / 2;
    std::nth_element(entries.begin(), entries.begin() + mid, entries.end(),
              [](const CachedAudioFrame* a, const CachedAudioFrame* b) {
                  return a->lastAccess < b->lastAccess;
              });
    
    // 中央値より古いエントリを削除
    for (int i = 0; i < mid; ++i) {
        cache_.remove(entries[i]->frameNumber);
    }
    
    qDebug() << "[AudioCache] LRU cleanup removed" << mid << "entries, remaining:" << cache_.size();
}

} // namespace ArtifactCore
