module;
#include <QObject>
#include <QImage>
#include <QString>
#include <future>
#include <memory>
#include <wobjectdefs.h>
#include "../../../include/Define/DllExportMacro.hpp"

export module IO.ImageExporter;

import Image.ExportOptions;

export namespace ArtifactCore {

struct ImageExportResult {
    bool success = false;
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
    ImageExportResult write(const QImage& image, const QString& filePath, const ImageExportOptions& options);
    std::future<ImageExportResult> writeAsync(const QImage& image, const QString& filePath, const ImageExportOptions& options);

    // Test methods (used in AppMain)
    ImageExportResult testWrite();
    ImageExportResult testWrite2();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
