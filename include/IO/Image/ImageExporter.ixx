module;
#include <utility>
#include <future>
#include <memory>
#include <OpenImageIO/imagebuf.h>
#include "../../../include/Define/DllExportMacro.hpp"
#include <QObject>
#include <QImage>
#include <QString>
#include <wobjectdefs.h>

export module IO.ImageExporter;

import Image.ExportOptions;

export namespace ArtifactCore {

struct ImageExportResult {
    bool success = false;
    QString errorStage;
    QString errorMessage;
    operator bool() const { return success; }
};

/**
 * @brief Manage and export images with many formats.
 */
class LIBRARY_DLL_API ImageExporter : public QObject {
    W_OBJECT(ImageExporter)
public:
    explicit ImageExporter(QObject* parent = nullptr);
    ~ImageExporter();

    // Image export methods
    ImageExportResult write(const OIIO::ImageBuf& image, const QString& filePath, const ImageExportOptions& options);
    ImageExportResult write(const QImage& image, const QString& filePath, const ImageExportOptions& options);
    std::future<ImageExportResult> writeAsync(const OIIO::ImageBuf& image, const QString& filePath, const ImageExportOptions& options);
    std::future<ImageExportResult> writeAsync(const QImage& image, const QString& filePath, const ImageExportOptions& options);

    // Test methods (used in AppMain)
    ImageExportResult testWrite();
    ImageExportResult testWrite2();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
