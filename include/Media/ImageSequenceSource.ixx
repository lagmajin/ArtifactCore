module;
#include <utility>
#include <QImage>
#include <QSize>
#include <QString>

#include "../Define/DllExportMacro.hpp"

export module Media.ImageSequenceSource;

import Media.ISource;

export namespace ArtifactCore {

class LIBRARY_DLL_API ImageSequenceSource final : public ISource {
public:
    ImageSequenceSource();
    ~ImageSequenceSource() override;

    bool open(const QString& uri) override;
    void close() override;
    bool isOpen() const override;

    SourceKind kind() const override;
    QString uri() const override;
    QString displayName() const override;
    SourceMetadata metadata() const override;

    bool seek(qint64 frameIndex) override;
    qint64 currentFrameIndex() const override;
    qint64 frameCount() const override;
    QSize frameSize() const override;
    double frameRate() const override;

    QImage frameAt(qint64 frameIndex) const override;

    void setFrameRate(double fps);

private:
    struct FrameEntry;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
