module;
class tst_QList;
#include <QDateTime>
#include <QDebug>
#include <QReadWriteLock>
#include <QVector>
#include <limits>

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
    QWriteLocker locker(&lock_);  // lastAccess mutation requires exclusive lock
    
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
    if (frameNumber < 0 || pcm.frameCount() <= 0 || pcm.channelCount() <= 0 ||
        pcm.channelData.size() < pcm.channelCount()) {
        return;
    }
    for (int channel = 0; channel < pcm.channelCount(); ++channel) {
        if (pcm.channelData[channel].size() < pcm.frameCount()) {
            return;
        }
    }
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
    if (startFrame < 0 || frameCount <= 0) {
        return;
    }
    PrefetchProvider provider;
    {
        QReadLocker locker(&lock_);
        provider = prefetchProvider_;
    }
    if (!provider) {
        return;
    }
    const int count = std::min(frameCount, 4096);
    const int64_t available = std::numeric_limits<int64_t>::max() - startFrame;
    const int safeCount = std::min<int64_t>(count, available + 1);
    if (safeCount <= 0) return;
    const int64_t endFrame = startFrame + static_cast<int64_t>(safeCount - 1);
    qDebug() << "[AudioCache] Prefetch requested:" << startFrame << "to" << endFrame;
    for (int index = 0; index < safeCount; ++index) {
        AudioSegment decoded;
        try {
            const int64_t frame = startFrame + static_cast<int64_t>(index);
            if (provider(frame, decoded)) {
                addCache(frame, std::move(decoded));
            }
        } catch (...) {
            qWarning() << "[AudioCache] Prefetch provider failed at index" << index;
        }
    }
}

void AudioCache::setPrefetchProvider(PrefetchProvider provider)
{
    QWriteLocker locker(&lock_);
    prefetchProvider_ = std::move(provider);
}

void AudioCache::clearExpired(qint64 maxAgeMs)
{
    QWriteLocker locker(&lock_);
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 safeAge = std::max<qint64>(0, maxAgeMs);
    const qint64 cutoffTime = safeAge > now
        ? std::numeric_limits<qint64>::min() : now - safeAge;
    
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

void AudioCache::cleanupLRU()
{
    if (cache_.isEmpty()) return;

    struct EntryTiming { int64_t key; qint64 lastAccess; };
    QVector<EntryTiming> entries;
    entries.reserve(cache_.size());

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        entries.append({it.key(), it->lastAccess});
    }

    const int mid = entries.size() / 2;
    std::nth_element(entries.begin(), entries.begin() + mid, entries.end(),
              [](const EntryTiming& a, const EntryTiming& b) {
                  return a.lastAccess < b.lastAccess;
              });

    for (int i = 0; i < mid; ++i) {
        cache_.remove(entries[i].key);
    }
}

} // namespace ArtifactCore
