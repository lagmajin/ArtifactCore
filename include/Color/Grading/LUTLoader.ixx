module;
#include <QImage>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <vector>
#include <algorithm>
#include <cmath>
#include <array>

export module Color.Grading.LUTLoader;

export namespace ArtifactCore {

// ============================================================
// 3D LUT
// ============================================================

class LUT3D {
public:
    bool loadFromFile(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "[LUT] Cannot open:" << path;
            return false;
        }

        QTextStream stream(&file);
        const QString ext = path.section('.', -1).toLower();

        if (ext == QStringLiteral("cube")) {
            return loadCube(stream);
        }

        qWarning() << "[LUT] Unsupported format:" << ext;
        return false;
    }

    // Apply LUT to an image
    QImage apply(const QImage& source) const {
        if (!isValid() || source.isNull()) return source;

        QImage result = source.convertToFormat(QImage::Format_RGBA8888);
        const int w = result.width();
        const int h = result.height();
        const float scale = static_cast<float>(size_ - 1);
        const float inv255 = 1.0f / 255.0f;

        for (int y = 0; y < h; ++y) {
            uint8_t* row = result.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int idx = x * 4;
                float r = row[idx] * inv255;
                float g = row[idx + 1] * inv255;
                float b = row[idx + 2] * inv255;

                // Trilinear interpolation
                float fr = r * scale;
                float fg = g * scale;
                float fb = b * scale;
                int ir = std::clamp(static_cast<int>(fr), 0, size_ - 2);
                int ig = std::clamp(static_cast<int>(fg), 0, size_ - 2);
                int ib = std::clamp(static_cast<int>(fb), 0, size_ - 2);
                float dr = fr - ir;
                float dg = fg - ig;
                float db = fb - ib;

                auto get = [&](int r, int g, int b) -> const float* {
                    return &data_[((r * size_ + g) * size_ + b) * 3];
                };

                const float* c000 = get(ir, ig, ib);
                const float* c100 = get(ir + 1, ig, ib);
                const float* c010 = get(ir, ig + 1, ib);
                const float* c110 = get(ir + 1, ig + 1, ib);
                const float* c001 = get(ir, ig, ib + 1);
                const float* c101 = get(ir + 1, ig, ib + 1);
                const float* c011 = get(ir, ig + 1, ib + 1);
                const float* c111 = get(ir + 1, ig + 1, ib + 1);

                auto lerp3 = [](const float* a, const float* b, float t) -> std::array<float, 3> {
                    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
                };

                auto c00 = lerp3(c000, c100, dr);
                auto c10 = lerp3(c010, c110, dr);
                auto c01 = lerp3(c001, c101, dr);
                auto c11 = lerp3(c011, c111, dr);

                auto c0 = lerp3(c00.data(), c10.data(), dg);
                auto c1 = lerp3(c01.data(), c11.data(), dg);
                auto c = lerp3(c0.data(), c1.data(), db);

                row[idx] = static_cast<uint8_t>(std::clamp(c[0] * 255.0f, 0.0f, 255.0f));
                row[idx + 1] = static_cast<uint8_t>(std::clamp(c[1] * 255.0f, 0.0f, 255.0f));
                row[idx + 2] = static_cast<uint8_t>(std::clamp(c[2] * 255.0f, 0.0f, 255.0f));
            }
        }

        return result;
    }

    bool isValid() const { return size_ > 0 && !data_.empty(); }
    int size() const { return size_; }
    QString name() const { return name_; }

private:
    int size_ = 0;
    std::vector<float> data_; // R-G-B order, size^3 * 3 floats
    QString name_;

    bool loadCube(QTextStream& stream) {
        data_.clear();
        size_ = 0;

        QString line;
        while (!stream.atEnd()) {
            line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;

            if (line.startsWith(QStringLiteral("TITLE"))) {
                name_ = line.mid(5).trimmed().remove('"');
                continue;
            }

            if (line.startsWith(QStringLiteral("LUT_3D_SIZE"))) {
                size_ = line.section(' ', -1).toInt();
                if (size_ < 2 || size_ > 256) {
                    qWarning() << "[LUT] Invalid size:" << size_;
                    return false;
                }
                data_.resize(static_cast<size_t>(size_) * size_ * size_ * 3, 0.0f);
                continue;
            }

            // RGB values
            if (size_ > 0) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    float r = parts[0].toFloat();
                    float g = parts[1].toFloat();
                    float b = parts[2].toFloat();

                    size_t idx = dataIdx_;
                    if (idx + 2 < data_.size()) {
                        data_[idx] = r;
                        data_[idx + 1] = g;
                        data_[idx + 2] = b;
                        dataIdx_ += 3;
                    }
                }
            }
        }

        if (dataIdx_ != data_.size()) {
            qWarning() << "[LUT] Incomplete data:" << dataIdx_ << "vs" << data_.size();
            return false;
        }

        return true;
    }

    size_t dataIdx_ = 0;
};

}
