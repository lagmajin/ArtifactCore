module;
#include <QImage>
#include <QString>
#include <QFuture>
#include <wobjectimpl.h>
#include <OpenImageIO/imageio.h>
#include <future>
#include <memory>
#include <thread>

module IO.ImageExporter;

import Image.ExportOptions;

namespace ArtifactCore {

class ImageExporter::Impl {
public:
    Impl() {}
    ~Impl() {}
};

W_OBJECT_IMPL(ImageExporter)

ImageExporter::ImageExporter(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>())
{
}

ImageExporter::~ImageExporter()
{
}

ImageExportResult ImageExporter::write(const QImage& image, const QString& filePath, const ImageExportOptions& options)
{
    using namespace OIIO;
    ImageExportResult result;
    
    std::unique_ptr<ImageOutput> out = ImageOutput::create(filePath.toStdString());
    if (!out) {
        result.success = false;
        result.errorMessage = "Could not create ImageOutput for: " + filePath;
        return result;
    }

    int nchannels = 4;
    ImageSpec spec(image.width(), image.height(), nchannels, TypeDesc::UINT8);
    
    if (!out->open(filePath.toStdString(), spec)) {
        result.success = false;
        result.errorMessage = "Could not open file: " + QString::fromStdString(out->geterror());
        return result;
    }

    if (!out->write_image(TypeDesc::UINT8, image.constBits())) {
        result.success = false;
        result.errorMessage = "Could not write image: " + QString::fromStdString(out->geterror());
        out->close();
        return result;
    }

    out->close();
    result.success = true;
    return result;
}

std::future<ImageExportResult> ImageExporter::writeAsync(const QImage& image, const QString& filePath, const ImageExportOptions& options)
{
    auto promise = std::make_shared<std::promise<ImageExportResult>>();
    std::future<ImageExportResult> future = promise->get_future();

    std::thread([this, image, filePath, options, promise]() {
        ImageExportResult result = write(image, filePath, options);
        promise->set_value(result);
    }).detach();

    return future;
}

ImageExportResult ImageExporter::testWrite()
{
    return write(QImage(100, 100, QImage::Format_RGBA8888), "test_output.png", ImageExportOptions());
}

ImageExportResult ImageExporter::testWrite2()
{
    ImageExportResult result;
    result.success = true;
    return result;
}

}