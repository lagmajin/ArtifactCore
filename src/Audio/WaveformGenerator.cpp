module;

#include <cmath>
#include <QPainter>
#include <QFile>
#include <QFileInfo>

export module Audio.WaveformGenerator;

import Audio.WaveformGenerator;

namespace ArtifactCore {

    // コンストラクタ
    WaveformGenerator::WaveformGenerator(QObject* parent)
        : QObject(parent)
    {
    }

    // デストラクタ
    WaveformGenerator::~WaveformGenerator() = default;

    // 波形タイプを設定
    void WaveformGenerator::setWaveformType(WaveformType type)
    {
        settings_.type = type;
    }

    // 波形スタイルを設定
    void WaveformGenerator::setWaveformStyle(WaveformStyle style)
    {
        settings_.style = style;
    }

    // 波形設定整体を設定
    void WaveformGenerator::setSettings(const WaveformSettings& settings)
    {
        settings_ = settings;
    }

    // 表示高さを設定
    void WaveformGenerator::setHeight(int height)
    {
        settings_.height = height;
    }

    // ピクセルあたりのサンプル数を設定
    void WaveformGenerator::setSamplesPerPixel(int samples)
    {
        settings_.samplesPerPixel = samples;
    }

    // 颜色を設定
    void WaveformGenerator::setPrimaryColor(const QColor& color)
    {
        settings_.primaryColor = color;
    }

    void WaveformGenerator::setSecondaryColor(const QColor& color)
    {
        settings_.secondaryColor = color;
    }

    void WaveformGenerator::setBackgroundColor(const QColor& color)
    {
        settings_.backgroundColor = color;
    }

    // 波形データを生成（生データから）
    WaveformData WaveformGenerator::generateData(const float* samples, size_t sampleCount, 
                                                int sampleRate, int channels)
    {
        return processAudioData(samples, sampleCount, sampleRate, channels);
    }

    // 波形データを生成（ファイルから）
    WaveformData WaveformGenerator::generateDataFromFile(const QString& audioFilePath)
    {
        WaveformData data;
        
        // チェック
        if (!QFile::exists(audioFilePath)) {
            return data;
        }
        
        // TODO: ファイルからオーディオを読み込む処理を実装
        // ここでは簡易的なStubを返す
        data.sampleRate = 48000;
        data.channelCount = 2;
        data.totalSamples = 0;
        data.duration = 0.0f;
        
        return data;
    }

    // 波形画像を生成（波形データから）
    WaveformResult WaveformGenerator::generateImage(const WaveformData& data, int width)
    {
        WaveformResult result;
        
        if (!data.isValid()) {
            result.errorMessage = "Invalid waveform data";
            return result;
        }
        
        int height = settings_.height;
        if (settings_.mirror) {
            height = height * 2;
        }
        
        // 画像を作成
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(settings_.backgroundColor);
        
        // ペインターを作成
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // タイプに応じて描画
        switch (settings_.type) {
        case WaveformType::Bars:
            drawBars(painter, data, width);
            break;
        case WaveformType::Lines:
            drawLines(painter, data, width);
            break;
        case WaveformType::Filled:
        case WaveformType::Mirrored:
            drawFilled(painter, data, width);
            break;
        default:
            drawBars(painter, data, width);
        }
        
        painter.end();
        
        result.image = image;
        result.success = true;
        result.width = width;
        result.height = height;
        
        return result;
    }

    // 波形画像を生成（生データから）
    WaveformResult WaveformGenerator::generateImage(const float* samples, size_t sampleCount, 
                                                   int width, int sampleRate, int channels)
    {
        WaveformData data = generateData(samples, sampleCount, sampleRate, channels);
        return generateImage(data, width);
    }

    // 波形画像を生成（ファイルから）
    WaveformResult WaveformGenerator::generateImageFromFile(const QString& audioFilePath, int width)
    {
        WaveformData data = generateDataFromFile(audioFilePath);
        return generateImage(data, width);
    }

    // 波形データをキャッシュ
    void WaveformGenerator::cacheWaveformData(const QString& key, const WaveformData& data)
    {
        // キャッシュがいっぱいの場合は古いものを削除
        if (cache_.size() >= maxCacheSize_) {
            cache_.remove(cache_.keys().first());
        }
        cache_[key] = data;
    }

    // キャッシュされた波形データを取得
    bool WaveformGenerator::getCachedData(const QString& key, WaveformData& data) const
    {
        if (cache_.contains(key)) {
            data = cache_[key];
            return true;
        }
        return false;
    }

    // キャッシュをクリア
    void WaveformGenerator::clearCache()
    {
        cache_.clear();
    }

    // キャッシュサイズを取得
    size_t WaveformGenerator::getCacheSize() const
    {
        return cache_.size();
    }

    // 推奨サンプルレートを取得
    int WaveformGenerator::getRecommendedSamplesPerPixel(int width, float duration)
    {
        if (duration <= 0 || width <= 0) {
            return 256;
        }
        
        int64_t totalSamples = static_cast<int64_t>(duration * 48000);
        return static_cast<int>(totalSamples / width);
    }

    // デフォルト設定を取得
    WaveformSettings WaveformGenerator::getDefaultSettings()
    {
        WaveformSettings settings;
        settings.type = WaveformType::Bars;
        settings.style = WaveformStyle::Modern;
        settings.height = 100;
        settings.samplesPerPixel = 256;
        settings.showRMS = true;
        settings.mirror = true;
        settings.primaryColor = QColor("#00A8E8");
        settings.secondaryColor = QColor("#007EA7");
        settings.backgroundColor = QColor("#1A1A2E");
        settings.peakColor = QColor("#FFFFFF");
        settings.showPeaks = true;
        settings.gradient = true;
        settings.barSpacing = 1;
        settings.autoScale = true;
        settings.quality = 1.0f;
        return settings;
    }

    // 非同期で波形画像を生成
    void WaveformGenerator::generateImageAsync(const QString& audioFilePath, int width)
    {
        // TODO: QtConcurrentを使用した非同期実装
        generateImageFromFile(audioFilePath, width);
    }

    // 非同期処理をキャンセル
    void WaveformGenerator::cancelAsyncGeneration()
    {
        // TODO: キャンセル処理の実装
    }

    // オーディオデータを処理
    WaveformData WaveformGenerator::processAudioData(const float* samples, size_t sampleCount, 
                                                     int sampleRate, int channels)
    {
        WaveformData data;
        
        if (!samples || sampleCount == 0) {
            return data;
        }
        
        data.sampleRate = sampleRate;
        data.channelCount = channels;
        data.totalSamples = sampleCount;
        data.duration = static_cast<float>(sampleCount) / static_cast<float>(sampleRate * channels);
        
        // モノラルに混合
        size_t monoSampleCount = sampleCount / channels;
        
        // ピクセルあたりのサンプル数を計算
        int samplesPerPixel = settings_.samplesPerPixel;
        if (samplesPerPixel <= 0) samplesPerPixel = 256;
        
        int outputWidth = static_cast<int>(monoSampleCount / samplesPerPixel);
        if (outputWidth <= 0) outputWidth = 1;
        
        data.minValues.resize(outputWidth);
        data.maxValues.resize(outputWidth);
        data.rmsValues.resize(outputWidth);
        data.peakValues.resize(outputWidth);
        
        for (int i = 0; i < outputWidth; ++i) {
            size_t startSample = i * samplesPerPixel * channels;
            size_t endSample = std::min(startSample + samplesPerPixel * channels, sampleCount);
            
            float minVal = 0.0f;
            float maxVal = 0.0f;
            float sumSquares = 0.0f;
            float peak = 0.0f;
            size_t count = 0;
            
            for (size_t j = startSample; j < endSample; j += channels) {
                // モノラル的平均
                float sample = 0.0f;
                for (int ch = 0; ch < channels && j + ch < sampleCount; ++ch) {
                    sample += samples[j + ch];
                }
                sample /= channels;
                
                if (sample < minVal) minVal = sample;
                if (sample > maxVal) maxVal = sample;
                
                float absSample = std::abs(sample);
                if (absSample > peak) peak = absSample;
                
                sumSquares += sample * sample;
                ++count;
            }
            
            data.minValues[i] = minVal;
            data.maxValues[i] = maxVal;
            data.rmsValues[i] = count > 0 ? std::sqrt(sumSquares / count) : 0.0f;
            data.peakValues[i] = peak;
        }
        
        return data;
    }

    // 棒グラフを描画
    void WaveformGenerator::drawBars(QPainter& painter, const WaveformData& data, int width)
    {
        int height = settings_.height;
        if (settings_.mirror) {
            height = height * 2;
        }
        
        int barCount = static_cast<int>(data.maxValues.size());
        if (barCount == 0) return;
        
        float barWidth = static_cast<float>(width) / barCount - settings_.barSpacing;
        if (barWidth < 1.0f) barWidth = 1.0f;
        
        float centerY = height / 2.0f;
        
        for (int i = 0; i < barCount; ++i) {
            float x = i * (barWidth + settings_.barSpacing);
            
            float maxVal = std::abs(data.maxValues[i]);
            float minVal = std::abs(data.minValues[i]);
            float rmsVal = data.rmsValues[i];
            
            int barHeight = static_cast<int>(maxVal * height / 2.0f);
            int barHeightMin = static_cast<int>(minVal * height / 2.0f);
            int rmsHeight = static_cast<int>(rmsVal * height / 2.0f);
            
            if (settings_.mirror) {
                // 上半分
                painter.fillRect(x, centerY - barHeight, barWidth, barHeight, settings_.primaryColor);
                // 下半分
                painter.fillRect(x, centerY, barWidth, barHeightMin, settings_.primaryColor);
                
                if (settings_.showRMS) {
                    painter.fillRect(x, centerY - rmsHeight, barWidth, rmsHeight, settings_.secondaryColor);
                    painter.fillRect(x, centerY, barWidth, rmsHeight, settings_.secondaryColor);
                }
            } else {
                painter.fillRect(x, height - barHeight, barWidth, barHeight, settings_.primaryColor);
                
                if (settings_.showRMS) {
                    painter.fillRect(x, height - rmsHeight, barWidth, rmsHeight, settings_.secondaryColor);
                }
            }
        }
    }

    // ラインを描画
    void WaveformGenerator::drawLines(QPainter& painter, const WaveformData& data, int width)
    {
        int height = settings_.height;
        
        int pointCount = static_cast<int>(data.maxValues.size());
        if (pointCount == 0) return;
        
        float stepX = static_cast<float>(width) / pointCount;
        
        QPen pen(settings_.primaryColor);
        pen.setWidthF(1.5f);
        painter.setPen(pen);
        
        // 上側ライン
        QPolygonF topPoints;
        for (int i = 0; i < pointCount; ++i) {
            float x = i * stepX;
            float y = height - (data.maxValues[i] * height);
            topPoints.append(QPointF(x, y));
        }
        painter.drawPolyline(topPoints);
        
        // 下側ライン
        QPolygonF bottomPoints;
        for (int i = 0; i < pointCount; ++i) {
            float x = i * stepX;
            float y = height - (data.minValues[i] * height);
            bottomPoints.append(QPointF(x, y));
        }
        painter.drawPolyline(bottomPoints);
        
        if (settings_.mirror) {
            // ミラー表示
            QPen pen2(settings_.secondaryColor);
            pen2.setWidthF(1.0f);
            painter.setPen(pen2);
            
            // 上側（下向き）
            QPolygonF topPoints2;
            for (int i = 0; i < pointCount; ++i) {
                float x = i * stepX;
                float y = data.maxValues[i] * height;
                topPoints2.append(QPointF(x, y));
            }
            painter.drawPolyline(topPoints2);
            
            // 下側（上向き）
            QPolygonF bottomPoints2;
            for (int i = 0; i < pointCount; ++i) {
                float x = i * stepX;
                float y = data.minValues[i] * height;
                bottomPoints2.append(QPointF(x, y));
            }
            painter.drawPolyline(bottomPoints2);
        }
    }

    // 塗りつぶしを描画
    void WaveformGenerator::drawFilled(QPainter& painter, const WaveformData& data, int width)
    {
        int height = settings_.height;
        
        int pointCount = static_cast<int>(data.maxValues.size());
        if (pointCount == 0) return;
        
        float stepX = static_cast<float>(width) / pointCount;
        
        if (settings_.mirror) {
            // ミラー塗りつぶし
            QPolygonF topPolygon;
            QPolygonF bottomPolygon;
            
            float centerY = height / 2.0f;
            
            for (int i = 0; i < pointCount; ++i) {
                float x = i * stepX;
                
                // 上側
                float topY = centerY - (data.maxValues[i] * height / 2.0f);
                topPolygon.append(QPointF(x, topY));
                
                // 下側
                float bottomY = centerY + (data.minValues[i] * height / 2.0f);
                bottomPolygon.append(QPointF(x, bottomY));
            }
            
            // 閉じる
            topPolygon.append(QPointF(width, centerY));
            topPolygon.append(QPointF(0, centerY));
            
            bottomPolygon.append(QPointF(width, centerY));
            bottomPolygon.append(QPointF(0, centerY));
            
            // グラデーション
            if (settings_.gradient) {
                QLinearGradient gradient(0, 0, 0, height);
                gradient.setColorAt(0, settings_.primaryColor);
                gradient.setColorAt(1, settings_.secondaryColor);
                painter.fillPolygon(topPolygon, gradient);
                painter.fillPolygon(bottomPolygon, gradient);
            } else {
                painter.fillPolygon(topPolygon, settings_.primaryColor);
                painter.fillPolygon(bottomPolygon, settings_.primaryColor);
            }
        } else {
            // 通常塗りつぶし
            QPolygonF polygon;
            
            for (int i = 0; i < pointCount; ++i) {
                float x = i * stepX;
                float y = height - (data.maxValues[i] * height);
                polygon.append(QPointF(x, y));
            }
            
            // 閉じる
            polygon.append(QPointF(width, height));
            polygon.append(QPointF(0, height));
            
            if (settings_.gradient) {
                QLinearGradient gradient(0, 0, 0, height);
                gradient.setColorAt(0, settings_.primaryColor);
                gradient.setColorAt(1, settings_.secondaryColor);
                painter.fillPolygon(polygon, gradient);
            } else {
                painter.fillPolygon(polygon, settings_.primaryColor);
            }
        }
    }

    // WaveformTypeから名称を取得
    QString waveformTypeToName(WaveformType type)
    {
        switch (type) {
        case WaveformType::Bars: return "Bars";
        case WaveformType::Lines: return "Lines";
        case WaveformType::Filled: return "Filled";
        case WaveformType::Mirrored: return "Mirrored";
        default: return "Unknown";
        }
    }

    // WaveformStyleから名称を取得
    QString waveformStyleToName(WaveformStyle style)
    {
        switch (style) {
        case WaveformStyle::Classic: return "Classic";
        case WaveformStyle::Modern: return "Modern";
        case WaveformStyle::Minimal: return "Minimal";
        default: return "Unknown";
        }
    }

    // 色を16進数文字列に変換
    QString colorToHex(const QColor& color)
    {
        return QString("#%1%2%3")
            .arg(color.red(), 2, 16, QChar('0'))
            .arg(color.green(), 2, 16, QChar('0'))
            .arg(color.blue(), 2, 16, QChar('0'));
    }

    // 16進数文字列から色に変換
    QColor hexToColor(const QString& hex)
    {
        if (hex.length() >= 7) {
            return QColor(hex.mid(1, 2).toInt(nullptr, 16),
                         hex.mid(3, 2).toInt(nullptr, 16),
                         hex.mid(5, 2).toInt(nullptr, 16));
        }
        return QColor();
    }

} // namespace ArtifactCore
