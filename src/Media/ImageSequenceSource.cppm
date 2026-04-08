module;
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include "../Define/DllExportMacro.hpp"

module Media.ImageSequenceSource;

import std;

namespace ArtifactCore {

struct ImageSequenceSource::FrameEntry {
    qint64 frameIndex = 0;
    QString path;
    QSize size;
};

struct ImageSequenceSource::Impl {
    QString uri;
    QString displayName;
    QVector<FrameEntry> frames;
    QSize frameSize;
    double frameRate = 24.0;
    qint64 currentFrameIndex = 0;
    bool open = false;
};

namespace {

bool isSupportedImageFile(const QFileInfo& info)
{
    if (!info.isFile()) {
        return false;
    }

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

bool parseSequencePattern(const QString& fileName, QString* prefix, QString* suffix)
{
    static const QRegularExpression rx(QStringLiteral("^(.*?)(\\d+)(\\.[^.]+)$"));
    const auto match = rx.match(fileName);
    if (!match.hasMatch()) {
        return false;
    }

    if (prefix) *prefix = match.captured(1);
    if (suffix) *suffix = match.captured(3);
    return true;
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

        const bool hasNumericPattern = parseSequencePattern(info.fileName(), &prefix, &suffix);
        const QDir dir = info.dir();

        if (hasNumericPattern) {
            const QString escapedPrefix = QRegularExpression::escape(prefix);
            const QString escapedSuffix = QRegularExpression::escape(suffix);
            const QString pattern = QStringLiteral("^%1(\\d+)%2$").arg(escapedPrefix, escapedSuffix);
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
    impl_->frameRate = 24.0;
    impl_->currentFrameIndex = 0;
    impl_->open = true;

    QImageReader reader(impl_->frames.front().path);
    impl_->frameSize = reader.size().isValid() ? reader.size() : QImage(impl_->frames.front().path).size();
    if (impl_->frameSize.isEmpty()) {
        impl_->frameSize = QSize(0, 0);
    }

    return true;
}

void ImageSequenceSource::close()
{
    impl_->uri.clear();
    impl_->displayName.clear();
    impl_->frames.clear();
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
    return true;
}

qint64 ImageSequenceSource::currentFrameIndex() const
{
    return impl_ ? impl_->currentFrameIndex : 0;
}

qint64 ImageSequenceSource::frameCount() const
{
    return impl_ ? impl_->frames.size() : 0;
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
    QImageReader reader(entry.path);
    QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }
    return image;
}

void ImageSequenceSource::setFrameRate(double fps)
{
    if (!impl_) {
        return;
    }

    impl_->frameRate = fps > 0.0 ? fps : 24.0;
}

} // namespace ArtifactCore
