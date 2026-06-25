module;
#include <utility>
#include <cstdint>
#include <QImage>
#include <QPainter>
#include <vector>
#include <algorithm>
#include <cmath>

export module Color.Grading.ColorScopes;

export namespace ArtifactCore {

// ============================================================
// Scope Type
// ============================================================

enum class ScopeType {
    Waveform,       // 輝度ウェーブフォーム
    Vectorscope,    // 色相/彩度ベクトルスコープ
    Histogram,      // 輝度ヒストグラム
    Parade          // RGB パレード
};

// ============================================================
// Color Scope Renderer
// ============================================================

class ColorScopeRenderer {
public:
    // Render waveform scope from an image
    static QImage renderWaveform(const QImage& source, int width = 256, int height = 128) {
        QImage scope(width, height, QImage::Format_RGBA8888);
        scope.fill(QColor(0, 0, 0, 220));

        if (source.isNull()) return scope;

        QImage img = source.convertToFormat(QImage::Format_RGBA8888);
        const int sw = img.width();
        const int sh = img.height();

        // Build waveform data: brightness at each horizontal position
        std::vector<std::vector<int>> waveform(width, std::vector<int>(height, 0));

        for (int y = 0; y < sh; ++y) {
            const uint8_t* row = img.constScanLine(y);
            for (int x = 0; x < sw; ++x) {
                int idx = x * 4;
                // Luminance: 0.299R + 0.587G + 0.114B
                int lum = static_cast<int>(0.299f * row[idx] + 0.587f * row[idx + 1] + 0.114f * row[idx + 2]);
                int sx = (x * width) / sw;
                int sy = (lum * (height - 1)) / 255;
                if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                    waveform[sx][sy]++;
                }
            }
        }

        // Render with intensity mapping
        int maxCount = 1;
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                maxCount = std::max(maxCount, waveform[x][y]);
            }
        }

        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (waveform[x][y] > 0) {
                    float intensity = static_cast<float>(waveform[x][y]) / static_cast<float>(maxCount);
                    int val = static_cast<int>(intensity * 255);
                    scope.setPixelColor(x, height - 1 - y, QColor(val, val, 255, val));
                }
            }
        }

        // Draw grid lines
        QPainter painter(&scope);
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        for (int i = 0; i <= 4; ++i) {
            int gy = (height * i) / 4;
            painter.drawLine(0, gy, width, gy);
        }
        painter.end();

        return scope;
    }

    // Render vectorscope from an image
    static QImage renderVectorscope(const QImage& source, int size = 128) {
        QImage scope(size, size, QImage::Format_RGBA8888);
        scope.fill(QColor(0, 0, 0, 220));

        if (source.isNull()) return scope;

        QImage img = source.convertToFormat(QImage::Format_RGBA8888);
        const int sw = img.width();
        const int sh = img.height();
        const int cx = size / 2;
        const int cy = size / 2;
        const float radius = static_cast<float>(size) * 0.45f;

        // Draw reference circle
        QPainter painter(&scope);
        painter.setPen(QPen(QColor(60, 60, 60), 1));
        painter.drawEllipse(cx - static_cast<int>(radius), cy - static_cast<int>(radius),
                            static_cast<int>(radius * 2), static_cast<int>(radius * 2));

        // Plot color samples
        for (int y = 0; y < sh; y += 2) {
            const uint8_t* row = img.constScanLine(y);
            for (int x = 0; x < sw; x += 2) {
                int idx = x * 4;
                float r = row[idx] / 255.0f;
                float g = row[idx + 1] / 255.0f;
                float b = row[idx + 2] / 255.0f;

                // Convert to CbCr (YCbCr chroma)
                float cb = -0.169f * r - 0.331f * g + 0.500f * b;
                float cr = 0.500f * r - 0.419f * g - 0.081f * b;

                int px = cx + static_cast<int>(cb * radius * 2.0f);
                int py = cy - static_cast<int>(cr * radius * 2.0f);

                if (px >= 0 && px < size && py >= 0 && py < size) {
                    QColor orig(static_cast<int>(row[idx]), static_cast<int>(row[idx + 1]),
                                static_cast<int>(row[idx + 2]), 180);
                    scope.setPixelColor(px, py, orig);
                }
            }
        }

        // Draw center cross
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        painter.drawLine(cx, 0, cx, size);
        painter.drawLine(0, cy, size, cy);
        painter.end();

        return scope;
    }

    // Render histogram from an image
    static QImage renderHistogram(const QImage& source, int width = 256, int height = 128) {
        QImage hist(width, height, QImage::Format_RGBA8888);
        hist.fill(QColor(0, 0, 0, 220));

        if (source.isNull()) return hist;

        QImage img = source.convertToFormat(QImage::Format_RGBA8888);
        const int sw = img.width();
        const int sh = img.height();

        // Build histograms for R, G, B, Luma
        std::array<int, 256> rHist{}, gHist{}, bHist{}, lHist{};

        for (int y = 0; y < sh; ++y) {
            const uint8_t* row = img.constScanLine(y);
            for (int x = 0; x < sw; ++x) {
                int idx = x * 4;
                rHist[row[idx]]++;
                gHist[row[idx + 1]]++;
                bHist[row[idx + 2]]++;
                int lum = static_cast<int>(0.299f * row[idx] + 0.587f * row[idx + 1] + 0.114f * row[idx + 2]);
                lHist[std::clamp(lum, 0, 255)]++;
            }
        }

        // Find max for scaling
        int maxVal = 1;
        for (int i = 0; i < 256; ++i) {
            maxVal = std::max({maxVal, rHist[i], gHist[i], bHist[i], lHist[i]});
        }

        // Render
        QPainter painter(&hist);
        painter.setPen(Qt::NoPen);

        auto drawChannel = [&](const std::array<int, 256>& data, QColor color, int alpha) {
            for (int x = 0; x < width; ++x) {
                int bin = (x * 255) / (width - 1);
                int h = (data[bin] * (height - 4)) / maxVal;
                if (h > 0) {
                    painter.setBrush(QColor(color.red(), color.green(), color.blue(), alpha));
                    painter.drawRect(x, height - h, 1, h);
                }
            }
        };

        drawChannel(rHist, QColor(255, 80, 80), 160);
        drawChannel(gHist, QColor(80, 255, 80), 160);
        drawChannel(bHist, QColor(80, 80, 255), 160);
        drawChannel(lHist, QColor(200, 200, 200), 100);

        // Grid
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        for (int i = 0; i <= 4; ++i) {
            int gx = (width * i) / 4;
            painter.drawLine(gx, 0, gx, height);
        }
        for (int i = 0; i <= 4; ++i) {
            int gy = (height * i) / 4;
            painter.drawLine(0, gy, width, gy);
        }

        painter.end();
        return hist;
    }

    // ============================================================
    // GPU bin-data renderers (consume raw uint32[] from ScopeComputer)
    // ============================================================

    /// Render vectorscope from GPU-computed bin data.
    /// bins must be size*size uint32_t values (count per scope coordinate).
    static QImage renderVectorscopeFromBins(const uint32_t* bins, int size) {
        QImage scope(size, size, QImage::Format_ARGB32_Premultiplied);
        scope.fill(QColor(0, 0, 0, 220));

        if (!bins || size <= 0) return scope;

        // Find max bin for normalization
        uint32_t maxBin = 1;
        for (int i = 0; i < size * size; ++i) {
            if (bins[i] > maxBin) maxBin = bins[i];
        }

        const float invMax = 1.0f / static_cast<float>(maxBin);

        // Render bins as intensity dots
        for (int y = 0; y < size; ++y) {
            auto* line = reinterpret_cast<QRgb*>(scope.scanLine(y));
            for (int x = 0; x < size; ++x) {
                uint32_t count = bins[y * size + x];
                if (count == 0) continue;
                float intensity = static_cast<float>(count) * invMax;
                int val = static_cast<int>(intensity * 200.0f + 55.0f);
                val = std::min(val, 255);
                QRgb existing = line[x];
                int a = std::min(255, qAlpha(existing) + val);
                int r = std::min(255, qRed(existing) + val);
                int g = std::min(255, qGreen(existing) + val);
                int b = std::min(255, qBlue(existing) + val);
                line[x] = qRgba(r, g, b, a);
            }
        }

        // Graticule
        QPainter painter(&scope);
        painter.setRenderHint(QPainter::Antialiasing);
        const int cx = size / 2;
        const int cy = size / 2;
        const float radius = static_cast<float>(size) * 0.45f;

        painter.setPen(QPen(QColor(60, 60, 60), 1));
        painter.drawEllipse(QPointF(cx, cy), radius, radius);
        painter.setPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
        painter.drawEllipse(QPointF(cx, cy), radius * 0.75f, radius * 0.75f);
        painter.drawEllipse(QPointF(cx, cy), radius * 0.50f, radius * 0.50f);
        painter.drawEllipse(QPointF(cx, cy), radius * 0.25f, radius * 0.25f);
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        painter.drawLine(cx, 0, cx, size);
        painter.drawLine(0, cy, size, cy);

        // Color target labels
        struct Target { const char* name; float cb; float cr; QColor color; };
        const Target targets[] = {
            {"R", -0.169f, 0.500f, QColor(180, 60, 60)},
            {"G", -0.331f, -0.419f, QColor(60, 180, 60)},
            {"B", 0.500f, -0.081f, QColor(60, 60, 180)},
            {"Cy", 0.331f, -0.500f, QColor(60, 180, 180)},
            {"Mg", 0.169f, 0.419f, QColor(180, 60, 180)},
            {"Yl", -0.500f, 0.081f, QColor(180, 180, 60)},
        };
        const float r90 = radius * 0.9f;
        painter.setFont(QFont("Consolas", 7));
        for (const auto& t : targets) {
            int tx = cx + static_cast<int>(t.cb * r90 * 2.0f);
            int ty = cy - static_cast<int>(t.cr * r90 * 2.0f);
            painter.setPen(QPen(t.color, 1));
            painter.drawRect(tx - 3, ty - 3, 6, 6);
            painter.drawText(tx + 6, ty + 3, t.name);
        }

        painter.end();
        return scope;
    }

    /// Render waveform from GPU-computed bin data.
    /// bins must be width*height uint32_t values (count per column/luma).
    static QImage renderWaveformFromBins(const uint32_t* bins,
                                         int width, int height) {
        QImage scope(width, height, QImage::Format_ARGB32_Premultiplied);
        scope.fill(QColor(0, 0, 0, 220));

        if (!bins || width <= 0 || height <= 0) return scope;

        uint32_t maxBin = 1;
        for (int i = 0; i < width * height; ++i) {
            if (bins[i] > maxBin) maxBin = bins[i];
        }

        const float invMax = 1.0f / static_cast<float>(maxBin);

        for (int y = 0; y < height; ++y) {
            auto* line = reinterpret_cast<QRgb*>(scope.scanLine(y));
            for (int x = 0; x < width; ++x) {
                uint32_t count = bins[y * width + x];
                if (count == 0) continue;
                float intensity = static_cast<float>(count) * invMax;
                int val = static_cast<int>(intensity * 255);
                val = std::min(val, 255);
                line[x] = qRgba(val, val, 255, val);
            }
        }

        QPainter painter(&scope);
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        for (int i = 0; i <= 4; ++i) {
            int gy = (height * i) / 4;
            painter.drawLine(0, gy, width, gy);
        }
        painter.end();
        return scope;
    }

    /// Render parade from GPU-computed bin data.
    /// bins: [R pane][G pane][B pane], each pane is width*height uint32_t.
    /// Total buffer size: 3 * width * height.
    static QImage renderParadeFromBins(const uint32_t* bins,
                                       int width, int height) {
        int totalW = width * 3;
        QImage scope(totalW, height, QImage::Format_ARGB32_Premultiplied);
        scope.fill(QColor(0, 0, 0, 220));

        if (!bins || width <= 0 || height <= 0) return scope;

        uint32_t maxBin = 1;
        for (int i = 0; i < 3 * width * height; ++i) {
            if (bins[i] > maxBin) maxBin = bins[i];
        }

        const float invMax = 1.0f / static_cast<float>(maxBin);
        const int pane = width * height;

        auto drawPane = [&](int paneOffset, QRgb color) {
            for (int y = 0; y < height; ++y) {
                auto* line = reinterpret_cast<QRgb*>(scope.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    uint32_t count = bins[paneOffset + y * width + x];
                    if (count == 0) continue;
                    float intensity = static_cast<float>(count) * invMax;
                    int a = std::min(255, static_cast<int>(intensity * 255));
                    line[x + paneOffset / pane * width] = qRgba(
                        qRed(color), qGreen(color), qBlue(color), a);
                }
            }
        };

        drawPane(0 * pane, qRgba(255, 80, 80, 255));
        drawPane(1 * pane, qRgba(80, 255, 80, 255));
        drawPane(2 * pane, qRgba(80, 80, 255, 255));

        QPainter painter(&scope);
        painter.setPen(QPen(QColor(80, 80, 80, 120), 1));
        for (int i = 0; i <= 4; ++i) {
            int gy = (height * i) / 4;
            painter.drawLine(0, gy, totalW, gy);
        }
        for (int p = 1; p < 3; ++p) {
            painter.drawLine(p * width, 0, p * width, height);
        }
        painter.end();
        return scope;
    }
};

}
