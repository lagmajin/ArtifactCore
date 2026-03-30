module;
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
};

}
