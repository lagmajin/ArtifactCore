module;
class tst_QList;
#include <utility>
#include <algorithm>
#include <memory>
#include <limits>
#include <iterator>
#include <atomic>
#include <mutex>

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QThreadPool>
#include <QVector>

#include "../Define/DllExportMacro.hpp"

module Media.ImageSequenceSource;

import Memory.TrackedPtr;
import Thread.Helper;

namespace ArtifactCore {

struct ImageSequenceSource::FrameEntry {
    qint64 frameIndex = 0;
    QString path;
    QSize size;
    bool fileExists = true;
};

constexpr int kFrameCacheCapacity = 8;
constexpr int kMaxPrefetchInFlight = 4;
constexpr quint64 kFrameCacheByteCapacity = 256ull * 1024ull * 1024ull;

struct ImageSequenceSource::Impl {
    struct CachedFrame {
        QImage image;
        qint64 fileSize = -1;
        qint64 lastModifiedMs = 0;
    };

    struct PrefetchedFrame {
        QImage image;
        qint64 fileSize = -1;
        qint64 lastModifiedMs = 0;
    };

    struct AsyncPrefetchState {
        std::mutex mutex;
        std::atomic_uint64_t generation{1};
        QHash<qint64, PrefetchedFrame> completed;
        QList<qint64> completedOrder;
        QSet<qint64> inFlight;
    };

    QString uri;
    QString displayName;
    QVector<FrameEntry> frames;
    QSize frameSize;
    double frameRate = 24.0;
    qint64 currentFrameIndex = 0;
    qint64 missingFrameCount = 0;
    QHash<qint64, CachedFrame> frameCache;
    QList<qint64> frameCacheOrder;
    mutable std::mutex frameCacheMutex;
    quint64 frameCacheBytes = 0;
    quint64 frameCacheHits = 0;
    quint64 frameCacheMisses = 0;
    std::shared_ptr<AsyncPrefetchState> prefetchState =
        std::make_shared<AsyncPrefetchState>();
    bool open = false;

    void resetDecodedCache()
    {
        const std::scoped_lock lock(frameCacheMutex);
        frameCache.clear();
        frameCacheOrder.clear();
        frameCacheBytes = 0;
        frameCacheHits = 0;
        frameCacheMisses = 0;
    }

    void storeDecodedFrame(qint64 frameIndex, CachedFrame frame)
    {
        const std::scoped_lock lock(frameCacheMutex);
        if (const auto existing = frameCache.constFind(frameIndex);
            existing != frameCache.cend()) {
            frameCacheBytes -= std::min<quint64>(
                frameCacheBytes,
                static_cast<quint64>(existing->image.sizeInBytes()));
        }
        frameCacheBytes += static_cast<quint64>(frame.image.sizeInBytes());
        frameCache.insert(frameIndex, std::move(frame));
        frameCacheOrder.removeAll(frameIndex);
        frameCacheOrder.push_back(frameIndex);
        while (frameCacheOrder.size() > kFrameCacheCapacity ||
               frameCacheBytes > kFrameCacheByteCapacity) {
            const qint64 oldest = frameCacheOrder.takeFirst();
            if (const auto oldestFrame = frameCache.constFind(oldest);
                oldestFrame != frameCache.cend()) {
                frameCacheBytes -= std::min<quint64>(
                    frameCacheBytes,
                    static_cast<quint64>(oldestFrame->image.sizeInBytes()));
            }
            frameCache.remove(oldest);
        }
    }

    void noteFrameExistence(qint64 frameIndex, bool existsNow)
    {
        if (frameIndex < 0 || frameIndex >= frames.size()) {
            return;
        }
        auto& entry = frames[frameIndex];
        if (entry.fileExists == existsNow) {
            return;
        }
        entry.fileExists = existsNow;
        missingFrameCount += existsNow ? -1 : 1;
        if (missingFrameCount < 0) {
            missingFrameCount = 0;
        }
    }

    void recountMissingFrames()
    {
        missingFrameCount = 0;
        for (const auto& frame : frames) {
            if (!frame.fileExists) {
                ++missingFrameCount;
            }
        }
    }
};

namespace {

bool isSupportedImageFile(const QFileInfo& info)
{
    const auto suffix = info.suffix().toLower().toLatin1();
    if (suffix.isEmpty()) {
        return false;
    }

    static const auto formats = [] {
        QSet<QByteArray> set;
        const auto list = QImageReader::supportedImageFormats();
        for (const auto& fmt : list) {
            set.insert(fmt.toLower());
        }
        return set;
    }();

    return formats.contains(suffix);
}

bool parseSequencePattern(const QString& fileName, QString* prefix, QString* suffix,
                          int* padding)
{
    static const QRegularExpression rx(QStringLiteral("^(.*?)(\\d+)(\\.[a-zA-Z0-9]+)$"));
    const auto match = rx.match(fileName);
    if (!match.hasMatch()) {
        return false;
    }

    if (prefix) *prefix = match.captured(1);
    if (suffix) *suffix = match.captured(3);
    if (padding) *padding = match.captured(2).size();
    return true;
}

void resetPrefetchState(const auto& state)
{
    if (!state) {
        return;
    }
    state->generation.fetch_add(1, std::memory_order_acq_rel);
    const std::scoped_lock lock(state->mutex);
    state->completed.clear();
    state->completedOrder.clear();
    state->inFlight.clear();
}

} // namespace

ImageSequenceSource::ImageSequenceSource()
    : impl_(std::make_unique<Impl>())
{
}

ImageSequenceSource::~ImageSequenceSource() = default;

bool ImageSequenceSource::open(const QString& uri)
{
    close();

    const QFileInfo info(uri);
    if (!info.exists()) {
        return false;
    }

    QVector<FrameEntry> frames;
    if (info.isDir()) {
        impl_->uri = info.absoluteFilePath();
        impl_->displayName = info.dir().dirName();

        const auto entries = QDir(info.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        qint64 frameIndex = 0;
        for (const auto& candidate : entries) {
            if (!isSupportedImageFile(candidate)) {
                continue;
            }

            FrameEntry entry;
            entry.frameIndex = frameIndex++;
            entry.path = candidate.absoluteFilePath();
            frames.push_back(entry);
        }
    } else if (info.isFile()) {
        impl_->uri = info.absoluteFilePath();
        impl_->displayName = info.completeBaseName();

        QString prefix;
        QString suffix;
        int padding = 0;

        const bool hasNumericPattern = parseSequencePattern(
            info.fileName(), &prefix, &suffix, &padding);
        const QDir dir = info.dir();

        if (hasNumericPattern) {
            const QString escapedPrefix = QRegularExpression::escape(prefix);
            const QString escapedSuffix = QRegularExpression::escape(suffix);
            const QString pattern = QStringLiteral("^%1(\\d{%2})%3$")
                .arg(escapedPrefix)
                .arg(padding)
                .arg(escapedSuffix);
            const QRegularExpression rx(pattern);
            const auto entries = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
            for (const auto& candidate : entries) {
                if (!isSupportedImageFile(candidate)) {
                    continue;
                }
                const auto match = rx.match(candidate.fileName());
                if (!match.hasMatch()) {
                    continue;
                }

                bool ok = false;
                const qint64 frameIndex = match.captured(1).toLongLong(&ok);
                if (!ok) {
                    continue;
                }

                FrameEntry entry;
                entry.frameIndex = frameIndex;
                entry.path = candidate.absoluteFilePath();
                frames.push_back(entry);
            }
        } else {
            FrameEntry entry;
            entry.frameIndex = 0;
            entry.path = info.absoluteFilePath();
            frames.push_back(entry);
        }
    } else {
        return false;
    }

    if (frames.isEmpty()) {
        return false;
    }

    std::sort(frames.begin(), frames.end(), [](const FrameEntry& a, const FrameEntry& b) {
        if (a.frameIndex != b.frameIndex) {
            return a.frameIndex < b.frameIndex;
        }
        return a.path < b.path;
    });

    impl_->frames = std::move(frames);
    impl_->resetDecodedCache();
    resetPrefetchState(impl_->prefetchState);
    impl_->frameRate = 24.0;
    impl_->currentFrameIndex = 0;
    impl_->open = true;

    impl_->frameSize = QSize();
    for (const auto& frame : impl_->frames) {
        QImageReader reader(frame.path);
        QSize candidateSize = reader.size();
        if (!candidateSize.isValid() || candidateSize.isEmpty()) {
            candidateSize = QImage(frame.path).size();
        }
        if (candidateSize.isValid() && !candidateSize.isEmpty()) {
            impl_->frameSize = candidateSize;
            break;
        }
    }

    return true;
}

bool ImageSequenceSource::openFramePaths(const QStringList& framePaths)
{
    close();

    QVector<FrameEntry> frames;
    frames.reserve(framePaths.size());
    for (const QString& rawPath : framePaths) {
        const QFileInfo info(rawPath.trimmed());
        // Keep missing files in the logical sequence.  Dropping them here
        // compresses frame indices and makes a saved gap indistinguishable
        // from a shorter sequence; decode will report the missing frame at
        // the point where it is actually requested.
        if ((info.exists() && !info.isFile()) || !isSupportedImageFile(info)) {
            continue;
        }
        FrameEntry entry;
        entry.frameIndex = frames.size();
        entry.path = info.absoluteFilePath();
        entry.fileExists = info.exists() && info.isFile();
        frames.push_back(std::move(entry));
    }
    if (frames.isEmpty()) {
        return false;
    }

    impl_->uri = frames.front().path;
    impl_->displayName = QFileInfo(impl_->uri).completeBaseName();
    impl_->frames = std::move(frames);
    impl_->recountMissingFrames();
    impl_->resetDecodedCache();
    resetPrefetchState(impl_->prefetchState);
    impl_->frameRate = 24.0;
    impl_->currentFrameIndex = 0;
    impl_->open = true;
    impl_->frameSize = QSize();
    for (const auto& frame : impl_->frames) {
        QImageReader reader(frame.path);
        QSize candidateSize = reader.size();
        if (!candidateSize.isValid() || candidateSize.isEmpty()) {
            candidateSize = QImage(frame.path).size();
        }
        if (candidateSize.isValid() && !candidateSize.isEmpty()) {
            impl_->frameSize = candidateSize;
            break;
        }
    }
    return true;
}

void ImageSequenceSource::close()
{
    impl_->uri.clear();
    impl_->displayName.clear();
    impl_->frames.clear();
    impl_->missingFrameCount = 0;
    impl_->resetDecodedCache();
    resetPrefetchState(impl_->prefetchState);
    impl_->frameSize = QSize();
    impl_->currentFrameIndex = 0;
    impl_->frameRate = 24.0;
    impl_->open = false;
}

bool ImageSequenceSource::isOpen() const
{
    return impl_ && impl_->open && !impl_->frames.isEmpty();
}

SourceKind ImageSequenceSource::kind() const
{
    return SourceKind::ImageSequence;
}

QString ImageSequenceSource::uri() const
{
    return impl_ ? impl_->uri : QString();
}

QString ImageSequenceSource::displayName() const
{
    return impl_ ? impl_->displayName : QString();
}

SourceMetadata ImageSequenceSource::metadata() const
{
    SourceMetadata metadata;
    if (!impl_) {
        return metadata;
    }

    metadata.displayName = impl_->displayName;
    metadata.uri = impl_->uri;
    metadata.frameSize = impl_->frameSize;
    metadata.frameRate = impl_->frameRate;
    metadata.frameCount = impl_->frames.size();
    metadata.frameStart = impl_->frames.front().frameIndex;
    metadata.frameEnd = impl_->frames.back().frameIndex;
    if (metadata.frameEnd >= metadata.frameStart &&
        metadata.frameEnd - metadata.frameStart <
            std::numeric_limits<qint64>::max()) {
        const qint64 span = metadata.frameEnd - metadata.frameStart + 1;
        metadata.missingFrameCount =
            std::max<qint64>(0, span - metadata.frameCount);
    }
    // Explicit frame-path sequences retain missing entries so their logical
    // positions stay stable.  Those entries are counted through the tracked
    // missingFrameCount (seeded at open, refreshed on frame access); unlike
    // numeric filename gaps, they do not widen the frame-number span above.
    metadata.missingFrameCount += impl_->missingFrameCount;
    metadata.hasVideo = !impl_->frames.isEmpty();
    metadata.isSequence = impl_->frames.size() > 1;
    return metadata;
}

bool ImageSequenceSource::seek(qint64 frameIndex)
{
    if (!isOpen() || impl_->frames.isEmpty()) {
        return false;
    }

    if (frameIndex < 0 || frameIndex >= impl_->frames.size()) {
        return false;
    }

    impl_->currentFrameIndex = frameIndex;
    if (frameIndex + 1 < impl_->frames.size()) {
        prefetchFrame(frameIndex + 1);
    }
    return true;
}

bool ImageSequenceSource::seekSourceFrame(qint64 frameNumber)
{
    const qint64 sequenceIndex = sequenceIndexForSourceFrame(frameNumber);
    return sequenceIndex >= 0 && seek(sequenceIndex);
}

qint64 ImageSequenceSource::currentFrameIndex() const
{
    return impl_ ? impl_->currentFrameIndex : 0;
}

qint64 ImageSequenceSource::frameCount() const
{
    return impl_ ? impl_->frames.size() : 0;
}

qint64 ImageSequenceSource::sourceFrameNumberAt(qint64 sequenceIndex) const
{
    if (!impl_ || sequenceIndex < 0 ||
        sequenceIndex >= impl_->frames.size()) {
        return -1;
    }
    return impl_->frames.at(sequenceIndex).frameIndex;
}

qint64 ImageSequenceSource::sequenceIndexForSourceFrame(qint64 frameNumber) const
{
    if (!impl_) {
        return -1;
    }
    const auto it = std::lower_bound(
        impl_->frames.cbegin(), impl_->frames.cend(), frameNumber,
        [](const FrameEntry& entry, qint64 value) {
            return entry.frameIndex < value;
        });
    if (it != impl_->frames.cend() && it->frameIndex == frameNumber) {
        return std::distance(impl_->frames.cbegin(), it);
    }
    return -1;
}

qint64 ImageSequenceSource::frameIndexAtTime(qint64 timelineFrame,
                                              double timelineFrameRate) const
{
    if (!isOpen() || frameCount() <= 0) {
        return -1;
    }
    if (timelineFrame <= 0 || !std::isfinite(timelineFrameRate) ||
        timelineFrameRate <= 0.0 || !std::isfinite(impl_->frameRate) ||
        impl_->frameRate <= 0.0) {
        return std::clamp<qint64>(timelineFrame, 0, frameCount() - 1);
    }

    const double sourceFrame =
        std::floor((static_cast<double>(timelineFrame) / timelineFrameRate) *
                   impl_->frameRate);
    if (!std::isfinite(sourceFrame) ||
        sourceFrame >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return frameCount() - 1;
    }
    return std::clamp<qint64>(static_cast<qint64>(sourceFrame), 0,
                              frameCount() - 1);
}

QSize ImageSequenceSource::frameSize() const
{
    return impl_ ? impl_->frameSize : QSize();
}

double ImageSequenceSource::frameRate() const
{
    return impl_ ? impl_->frameRate : 0.0;
}

QImage ImageSequenceSource::frameAt(qint64 frameIndex) const
{
    if (!impl_ || impl_->frames.isEmpty()) {
        return {};
    }

    if (frameIndex < 0 || frameIndex >= impl_->frames.size()) {
        return {};
    }

    const auto& entry = impl_->frames.at(frameIndex);
    const QFileInfo sourceInfo(entry.path);
    const qint64 sourceSize = sourceInfo.size();
    const qint64 lastModifiedMs = sourceInfo.lastModified().toMSecsSinceEpoch();
    impl_->noteFrameExistence(frameIndex, sourceInfo.isFile());

    Impl::PrefetchedFrame prefetched;
    bool hasPrefetched = false;
    if (impl_->prefetchState) {
        const std::scoped_lock lock(impl_->prefetchState->mutex);
        const auto completed = impl_->prefetchState->completed.find(frameIndex);
        if (completed != impl_->prefetchState->completed.end()) {
            prefetched = std::move(completed.value());
            impl_->prefetchState->completed.erase(completed);
            impl_->prefetchState->completedOrder.removeAll(frameIndex);
            impl_->prefetchState->inFlight.remove(frameIndex);
            hasPrefetched = true;
        }
    }
    if (hasPrefetched && prefetched.fileSize == sourceSize &&
        prefetched.lastModifiedMs == lastModifiedMs) {
        Impl::CachedFrame cachedFrame;
        cachedFrame.image = std::move(prefetched.image);
        cachedFrame.fileSize = prefetched.fileSize;
        cachedFrame.lastModifiedMs = prefetched.lastModifiedMs;
        const QImage result = cachedFrame.image;
        impl_->storeDecodedFrame(frameIndex, std::move(cachedFrame));
        return result;
    }

    {
        const std::scoped_lock lock(impl_->frameCacheMutex);
        if (const auto cached = impl_->frameCache.constFind(frameIndex);
            cached != impl_->frameCache.cend()) {
            if (cached->fileSize == sourceSize &&
                cached->lastModifiedMs == lastModifiedMs) {
                ++impl_->frameCacheHits;
                impl_->frameCacheOrder.removeAll(frameIndex);
                impl_->frameCacheOrder.push_back(frameIndex);
                return cached->image;
            }
            // A same-path replacement invalidates the old decoded frame even when
            // the replacement later turns out to be unreadable.
            impl_->frameCache.remove(frameIndex);
            impl_->frameCacheOrder.removeAll(frameIndex);
        }

        ++impl_->frameCacheMisses;
    }

    QImageReader reader(entry.path);
    QImage image = reader.read();
    if (image.isNull()) {
        // Keep a negative result in the same bounded cache.  Broken or
        // temporarily unreadable frames should not be decoded again on every
        // scrub until the file changes.
        ImageSequenceSource::Impl::CachedFrame failedFrame;
        failedFrame.fileSize = sourceSize;
        failedFrame.lastModifiedMs = lastModifiedMs;
        impl_->storeDecodedFrame(frameIndex, std::move(failedFrame));
        return {};
    }
    ImageSequenceSource::Impl::CachedFrame cachedFrame;
    cachedFrame.image = image;
    cachedFrame.fileSize = sourceSize;
    cachedFrame.lastModifiedMs = lastModifiedMs;
    impl_->storeDecodedFrame(frameIndex, std::move(cachedFrame));
    if (impl_->prefetchState) {
        const std::scoped_lock lock(impl_->prefetchState->mutex);
        impl_->prefetchState->completed.remove(frameIndex);
        impl_->prefetchState->completedOrder.removeAll(frameIndex);
        impl_->prefetchState->inFlight.remove(frameIndex);
    }
    return image;
}

bool ImageSequenceSource::tryFrameAt(qint64 frameIndex, QImage& frame) const
{
    frame = {};
    if (!impl_ || !impl_->prefetchState || frameIndex < 0 ||
        frameIndex >= impl_->frames.size()) {
        return false;
    }

    const QFileInfo sourceInfo(impl_->frames.at(frameIndex).path);
    const qint64 sourceSize = sourceInfo.size();
    const qint64 lastModifiedMs = sourceInfo.lastModified().toMSecsSinceEpoch();
    impl_->noteFrameExistence(frameIndex, sourceInfo.isFile());
    Impl::PrefetchedFrame prefetched;
    bool hasPrefetched = false;
    {
        const std::scoped_lock lock(impl_->prefetchState->mutex);
        const auto completed = impl_->prefetchState->completed.find(frameIndex);
        if (completed != impl_->prefetchState->completed.end()) {
            prefetched = std::move(completed.value());
            impl_->prefetchState->completed.erase(completed);
            impl_->prefetchState->completedOrder.removeAll(frameIndex);
            hasPrefetched = true;
        }
    }
    if (hasPrefetched && prefetched.fileSize == sourceSize &&
        prefetched.lastModifiedMs == lastModifiedMs) {
        Impl::CachedFrame cached;
        cached.image = std::move(prefetched.image);
        cached.fileSize = prefetched.fileSize;
        cached.lastModifiedMs = prefetched.lastModifiedMs;
        frame = cached.image;
        impl_->storeDecodedFrame(frameIndex, std::move(cached));
        {
            const std::scoped_lock lock(impl_->frameCacheMutex);
            if (frame.isNull()) ++impl_->frameCacheMisses;
            else ++impl_->frameCacheHits;
        }
        return !frame.isNull();
    }

    {
        const std::scoped_lock lock(impl_->frameCacheMutex);
        const auto cached = impl_->frameCache.find(frameIndex);
        if (cached != impl_->frameCache.end()) {
            if (cached->fileSize == sourceSize &&
                cached->lastModifiedMs == lastModifiedMs) {
                frame = cached->image;
                impl_->frameCacheOrder.removeAll(frameIndex);
                impl_->frameCacheOrder.push_back(frameIndex);
                if (!frame.isNull()) ++impl_->frameCacheHits;
                return !frame.isNull();
            }
            impl_->frameCache.erase(cached);
            impl_->frameCacheOrder.removeAll(frameIndex);
        }
        ++impl_->frameCacheMisses;
    }
    prefetchFrame(frameIndex);
    return false;
}

void ImageSequenceSource::setFrameRate(double fps)
{
    if (!impl_) {
        return;
    }

    impl_->frameRate = fps > 0.0 ? fps : 24.0;
}

quint64 ImageSequenceSource::frameCacheHitCount() const
{
    if (!impl_) return 0;
    const std::scoped_lock lock(impl_->frameCacheMutex);
    return impl_->frameCacheHits;
}

quint64 ImageSequenceSource::frameCacheMissCount() const
{
    if (!impl_) return 0;
    const std::scoped_lock lock(impl_->frameCacheMutex);
    return impl_->frameCacheMisses;
}

int ImageSequenceSource::frameCacheEntryCount() const
{
    if (!impl_) return 0;
    const std::scoped_lock lock(impl_->frameCacheMutex);
    return impl_->frameCache.size();
}

int ImageSequenceSource::frameCacheCapacity() const
{
    return kFrameCacheCapacity;
}

quint64 ImageSequenceSource::frameCacheBytes() const
{
    if (!impl_) return 0;
    const std::scoped_lock lock(impl_->frameCacheMutex);
    return impl_->frameCacheBytes;
}

quint64 ImageSequenceSource::frameCacheByteCapacity() const
{
    return kFrameCacheByteCapacity;
}

void ImageSequenceSource::prefetchFrame(qint64 frameIndex) const
{
    if (!impl_ || !impl_->prefetchState || frameIndex < 0 ||
        frameIndex >= impl_->frames.size()) {
        return;
    }

    {
        const std::scoped_lock cacheLock(impl_->frameCacheMutex);
        if (impl_->frameCache.contains(frameIndex)) {
            return;
        }
    }

    const auto state = impl_->prefetchState;
    const std::uint64_t generation =
        state->generation.load(std::memory_order_acquire);
    const QString path = impl_->frames.at(frameIndex).path;
    {
        const std::scoped_lock lock(state->mutex);
        if (state->inFlight.contains(frameIndex) ||
            state->completed.contains(frameIndex)) {
            return;
        }
        if (state->inFlight.size() >= kMaxPrefetchInFlight) {
            return;
        }
        state->inFlight.insert(frameIndex);
    }

    sharedBackgroundThreadPool().start(
        [state, generation, frameIndex, path]() {
            ScopedThreadName threadName(
                QStringLiteral("ImageSequence/prefetch:%1")
                    .arg(QFileInfo(path).fileName()));
            const QFileInfo sourceInfo(path);
            QImageReader reader(path);
            Impl::PrefetchedFrame result;
            result.image = reader.read();
            result.fileSize = sourceInfo.size();
            result.lastModifiedMs =
                sourceInfo.lastModified().toMSecsSinceEpoch();

            const std::scoped_lock lock(state->mutex);
            if (state->generation.load(std::memory_order_acquire) != generation) {
                return;
            }
            state->inFlight.remove(frameIndex);
            state->completed.insert(frameIndex, std::move(result));
            state->completedOrder.removeAll(frameIndex);
            state->completedOrder.push_back(frameIndex);
            while (state->completedOrder.size() > kFrameCacheCapacity) {
                const qint64 oldest = state->completedOrder.takeFirst();
                state->completed.remove(oldest);
            }
        });
}

void ImageSequenceSource::clearFrameCache()
{
    if (!impl_) {
        return;
    }
    impl_->resetDecodedCache();
    resetPrefetchState(impl_->prefetchState);
}

} // namespace ArtifactCore
