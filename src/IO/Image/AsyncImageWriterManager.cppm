module;
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QByteArray>
#include <QString>

module IO.Async.ImageWriterManager;

import IO.ImageExporter;
import Image;

namespace ArtifactCore {

namespace {

float normalizeChannel(const RawImage& image, const uchar* srcBytes, std::size_t offset)
{
    const QString type = image.pixelType;
    if (type == QStringLiteral("uint8")) {
        return static_cast<float>(srcBytes[offset]) / 255.0f;
    }

    if (type == QStringLiteral("uint16") || type == QStringLiteral("half")) {
        quint16 value = 0;
        std::memcpy(&value, srcBytes + offset, sizeof(quint16));
        return static_cast<float>(value) / 65535.0f;
    }

    if (type == QStringLiteral("float")) {
        float value = 0.0f;
        std::memcpy(&value, srcBytes + offset, sizeof(float));
        return std::clamp(value, 0.0f, 1.0f);
    }

    if (type == QStringLiteral("double")) {
        double value = 0.0;
        std::memcpy(&value, srcBytes + offset, sizeof(double));
        return static_cast<float>(std::clamp(value, 0.0, 1.0));
    }

    if (type == QStringLiteral("int8")) {
        qint8 value = 0;
        std::memcpy(&value, srcBytes + offset, sizeof(qint8));
        return std::clamp((static_cast<float>(value) + 128.0f) / 255.0f, 0.0f, 1.0f);
    }

    if (type == QStringLiteral("int16")) {
        qint16 value = 0;
        std::memcpy(&value, srcBytes + offset, sizeof(qint16));
        return std::clamp((static_cast<float>(value) + 32768.0f) / 65535.0f, 0.0f, 1.0f);
    }

    return 0.0f;
}

QImage convertRawImageToQImage(const RawImage& image)
{
    if (!image.isValid() || image.width <= 0 || image.height <= 0) {
        return {};
    }

    const int pixelSize = image.getPixelTypeSizeInBytes();
    if (pixelSize <= 0 || image.channels <= 0) {
        return {};
    }

    QImage out(image.width, image.height, QImage::Format_RGBA8888);
    if (out.isNull()) {
        return {};
    }

    const uchar* srcBytes = reinterpret_cast<const uchar*>(image.data.constData());
    const std::size_t srcStride = static_cast<std::size_t>(image.channels) * static_cast<std::size_t>(pixelSize);

    for (int y = 0; y < image.height; ++y) {
        uchar* dstRow = out.scanLine(y);
        const uchar* srcRow = srcBytes + static_cast<std::size_t>(y) * srcStride * static_cast<std::size_t>(image.width);

        for (int x = 0; x < image.width; ++x) {
            const std::size_t pixelIndex = static_cast<std::size_t>(x) * srcStride;
            const float c0 = normalizeChannel(image, srcRow, pixelIndex);
            const float c1 = (image.channels > 1) ? normalizeChannel(image, srcRow, pixelIndex + pixelSize) : c0;
            const float c2 = (image.channels > 2) ? normalizeChannel(image, srcRow, pixelIndex + pixelSize * 2) : c0;
            const float c3 = (image.channels > 3) ? normalizeChannel(image, srcRow, pixelIndex + pixelSize * 3) : 1.0f;

            float r = c0;
            float g = c0;
            float b = c0;
            float a = 1.0f;
            if (image.channels == 2) {
                a = c1;
            } else if (image.channels >= 3) {
                g = c1;
                b = c2;
                a = c3;
            }

            dstRow[x * 4 + 0] = static_cast<uchar>(std::lround(std::clamp(r, 0.0f, 1.0f) * 255.0f));
            dstRow[x * 4 + 1] = static_cast<uchar>(std::lround(std::clamp(g, 0.0f, 1.0f) * 255.0f));
            dstRow[x * 4 + 2] = static_cast<uchar>(std::lround(std::clamp(b, 0.0f, 1.0f) * 255.0f));
            dstRow[x * 4 + 3] = static_cast<uchar>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
        }
    }

    return out;
}

} // namespace

class AsyncImageWriterManager::Impl {
private:
    boost::asio::thread_pool thread_pool_;
    std::atomic<bool> stop_processing_{false};

    void performImageWrite(std::string filepath_str, RawImagePtr image);

public:
    explicit Impl(std::size_t num_threads = std::thread::hardware_concurrency());
    ~Impl();

    void enqueueImageWrite(const QString& filepath, RawImagePtr image);
};

AsyncImageWriterManager::Impl::Impl(std::size_t num_threads)
    : thread_pool_(std::max<std::size_t>(1, num_threads))
{
}

AsyncImageWriterManager::Impl::~Impl()
{
    stop_processing_.store(true);
    thread_pool_.join();
}

void AsyncImageWriterManager::Impl::performImageWrite(std::string filepath_str, RawImagePtr image)
{
    if (stop_processing_.load()) {
        return;
    }

    const QString filepath = QString::fromUtf8(filepath_str.data(), static_cast<int>(filepath_str.size()));
    if (filepath.isEmpty() || !image) {
        return;
    }

    const QImage qimage = convertRawImageToQImage(*image);
    if (qimage.isNull()) {
        qWarning() << "AsyncImageWriterManager: failed to convert image for" << filepath;
        return;
    }

    ImageExporter exporter;
    const ImageExportResult result = exporter.write(qimage, filepath, ImageExportOptions{});
    if (!result.success) {
        qWarning() << "AsyncImageWriterManager: write failed for" << filepath
                   << "stage=" << result.errorStage
                   << "message=" << result.errorMessage;
    }
}

void AsyncImageWriterManager::Impl::enqueueImageWrite(const QString& filepath, RawImagePtr image)
{
    if (stop_processing_.load() || filepath.isEmpty() || !image) {
        return;
    }

    const QByteArray filepathUtf8 = filepath.toUtf8();
    boost::asio::post(thread_pool_, [this, filepathUtf8, image = std::move(image)]() mutable {
        performImageWrite(std::string(filepathUtf8.constData()), std::move(image));
    });
}

AsyncImageWriterManager::AsyncImageWriterManager()
    : impl_(new Impl())
{
}

AsyncImageWriterManager::~AsyncImageWriterManager()
{
    delete impl_;
    impl_ = nullptr;
}

void AsyncImageWriterManager::enqueueWriter(const QString& filepath, RawImagePtr image)
{
    if (!impl_) {
        return;
    }
    impl_->enqueueImageWrite(filepath, std::move(image));
}

bool AsyncImageWriterManager::hasRenderQueue() const
{
    return impl_ != nullptr;
}

} // namespace ArtifactCore
