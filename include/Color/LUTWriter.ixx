module;

#include <QString>
#include <QByteArray>

export module Color.LUTWriter;

export namespace ArtifactCore {

/// 3D LUT export formats
enum class LUTFormat {
    Cube,   // Adobe .cube (1D/3D)
    Discreet3DL // Discreet .3dl
};

/// Write a 3D LUT to file.
/// data: N^3 * 3 float values (RGB), size: LUT dimension (e.g. 17, 33, 65).
/// Returns true on success, false on error (errorMessage is set).
bool writeLUTToFile(
    const QString& path,
    const float* data,
    int size,
    LUTFormat format,
    QString* errorMessage = nullptr
);

/// Export LUT as QByteArray (for clipboard or preview).
QByteArray exportLUTToMemory(
    const float* data,
    int size,
    LUTFormat format
);

} // namespace ArtifactCore
