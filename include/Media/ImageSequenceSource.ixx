module;
#include <utility>
#include <QImage>
#include <QHash>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "../Define/DllExportMacro.hpp"

export module Media.ImageSequenceSource;

import Media.ISource;

export namespace ArtifactCore {

class LIBRARY_DLL_API ImageSequenceSource final : public ISource {
public:
    ImageSequenceSource();
    ~ImageSequenceSource() override;

    bool open(const QString& uri) override;
    bool openFramePaths(const QStringList& framePaths);
    void close() override;
    bool isOpen() const override;

    SourceKind kind() const override;
    QString uri() const override;
    QString displayName() const override;
    SourceMetadata metadata() const override;

    bool seek(qint64 frameIndex) override;
    bool seekSourceFrame(qint64 frameNumber);
    qint64 currentFrameIndex() const override;
    qint64 frameCount() const override;
    qint64 sourceFrameNumberAt(qint64 sequenceIndex) const;
    qint64 sequenceIndexForSourceFrame(qint64 frameNumber) const;
    // Resolve a composition/timeline frame into the sequence's zero-based
    // frame index using the timeline and source frame rates.
    qint64 frameIndexAtTime(qint64 timelineFrame, double timelineFrameRate) const;
    QSize frameSize() const override;
    double frameRate() const override;

    QImage frameAt(qint64 frameIndex) const override;
    bool tryFrameAt(qint64 frameIndex, QImage& frame) const;

    void setFrameRate(double fps);

    // Bounded decoded-frame cache diagnostics and control.
    quint64 frameCacheHitCount() const;
    quint64 frameCacheMissCount() const;
    int frameCacheEntryCount() const;
    int frameCacheCapacity() const;
    quint64 frameCacheBytes() const;
    quint64 frameCacheByteCapacity() const;
    void prefetchFrame(qint64 frameIndex) const;
    void clearFrameCache();

private:
    struct FrameEntry;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
