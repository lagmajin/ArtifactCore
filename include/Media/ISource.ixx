module;
#include <QImage>
#include <QSize>
#include <QString>

#include "../Define/DllExportMacro.hpp"

export module Media.ISource;

import std;

export namespace ArtifactCore {

enum class SourceKind {
    Unknown = 0,
    File,
    Generated,
    ImageSequence
};

struct LIBRARY_DLL_API SourceMetadata {
    QString displayName;
    QString uri;
    QSize frameSize;
    double frameRate = 0.0;
    qint64 frameCount = 0;
    bool hasVideo = false;
    bool hasAudio = false;
    bool isSequence = false;
};

class LIBRARY_DLL_API ISource {
public:
    virtual ~ISource() = default;

    virtual bool open(const QString& uri) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual SourceKind kind() const = 0;
    virtual QString uri() const = 0;
    virtual QString displayName() const = 0;
    virtual SourceMetadata metadata() const = 0;

    virtual bool seek(qint64 frameIndex) = 0;
    virtual qint64 currentFrameIndex() const = 0;
    virtual qint64 frameCount() const = 0;
    virtual QSize frameSize() const = 0;
    virtual double frameRate() const = 0;

    virtual QImage frameAt(qint64 frameIndex) const = 0;

    QImage currentFrame() const
    {
        return frameAt(currentFrameIndex());
    }
};

} // namespace ArtifactCore
