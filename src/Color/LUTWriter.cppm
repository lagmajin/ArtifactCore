module;

#include <QFile>
#include <QTextStream>
#include <QByteArray>
#include <cmath>

module Color.LUTWriter;

namespace ArtifactCore {

static QByteArray writeCubeFormat(const float* data, int size)
{
    QByteArray result;
    QTextStream out(&result);

    out << "TITLE \"ArtifactStudio Exported LUT\"\n";
    out << "LUT_3D_SIZE " << size << "\n";
    out << "DOMAIN_MIN 0.0 0.0 0.0\n";
    out << "DOMAIN_MAX 1.0 1.0 1.0\n";

    const int total = size * size * size;
    for (int i = 0; i < total; ++i) {
        const int idx = i * 3;
        out << QString("%1 %2 %3\n")
               .arg(data[idx], 0, 'f', 6)
               .arg(data[idx + 1], 0, 'f', 6)
               .arg(data[idx + 2], 0, 'f', 6);
    }
    return result;
}

static QByteArray writeDiscreet3dlFormat(const float* data, int size)
{
    QByteArray result;
    QTextStream out(&result);

    out << "3DMLUT\n";
    out << "LUT_3D_SIZE " << size << "\n";
    out << "\n";

    const int total = size * size * size;
    for (int i = 0; i < total; ++i) {
        const int idx = i * 3;
        out << QString("%1 %2 %3 %4\n")
               .arg(i, 4, 10, QChar('0'))
               .arg(data[idx], 0, 'f', 6)
               .arg(data[idx + 1], 0, 'f', 6)
               .arg(data[idx + 2], 0, 'f', 6);
    }
    return result;
}

QByteArray exportLUTToMemory(const float* data, int size, LUTFormat format)
{
    if (!data || size < 2 || size > 256) {
        return QByteArray();
    }
    switch (format) {
    case LUTFormat::Cube:
        return writeCubeFormat(data, size);
    case LUTFormat::Discreet3DL:
        return writeDiscreet3dlFormat(data, size);
    }
    return QByteArray();
}

bool writeLUTToFile(const QString& path, const float* data, int size,
                    LUTFormat format, QString* errorMessage)
{
    if (!data || size < 2 || size > 256) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid LUT data or size");
        return false;
    }

    const QByteArray content = exportLUTToMemory(data, size, format);
    if (content.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to generate LUT content");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        return false;
    }

    file.write(content);
    file.close();
    return true;
}

} // namespace ArtifactCore
