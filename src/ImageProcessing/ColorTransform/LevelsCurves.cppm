module;

#include <QString>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <cmath>
#include <algorithm>
#include <vector>

module ImageProcessing.ColorTransform.LevelsCurves;

import std;

namespace ArtifactCore {

using namespace ColorAdjustmentUtils;

// ============================================================================
// LevelsSettings プリセット実装
// ============================================================================

LevelsSettings LevelsSettings::highContrast() {
    LevelsSettings s;
    s.inputBlack = 20.0;
    s.inputWhite = 235.0;
    s.inputGamma = 1.0;
    s.outputBlack = 0.0;
    s.outputWhite = 255.0;
    return s;
}

LevelsSettings LevelsSettings::lowContrast() {
    LevelsSettings s;
    s.inputBlack = 0.0;
    s.inputWhite = 255.0;
    s.inputGamma = 1.0;
    s.outputBlack = 30.0;
    s.outputWhite = 225.0;
    return s;
}

LevelsSettings LevelsSettings::brighten() {
    LevelsSettings s;
    s.inputBlack = 0.0;
    s.inputWhite = 255.0;
    s.inputGamma = 0.8;
    s.outputBlack = 0.0;
    s.outputWhite = 255.0;
    return s;
}

LevelsSettings LevelsSettings::darken() {
    LevelsSettings s;
    s.inputBlack = 0.0;
    s.inputWhite = 255.0;
    s.inputGamma = 1.25;
    s.outputBlack = 0.0;
    s.outputWhite = 255.0;
    return s;
}

LevelsSettings LevelsSettings::autoLevels(const QImage& image) {
    auto histogram = LevelsEffect::calculateHistogram(image);
    return calculateAutoLevels(histogram);
}

// ============================================================================
// LevelsEffect::Impl
// ============================================================================

class LevelsEffect::Impl {
public:
    LevelsSettings settings;
    QVector<quint8> lutR;
    QVector<quint8> lutG;
    QVector<quint8> lutB;
    bool lutValid = false;
    
    void buildLUT() {
        lutR.resize(256);
        lutG.resize(256);
        lutB.resize(256);
        
        for (int i = 0; i < 256; ++i) {
            double value = i / 255.0;
            double result = applyLevels(value, 
                settings.inputBlack, settings.inputWhite,
                settings.inputGamma, 
                settings.outputBlack, settings.outputWhite);
            
            quint8 lutValue = static_cast<quint8>(std::clamp(result * 255.0, 0.0, 255.0));
            lutR[i] = lutValue;
            lutG[i] = lutValue;
            lutB[i] = lutValue;
        }
        
        // チャンネルごとの設定
        if (settings.perChannel) {
            for (int i = 0; i < 256; ++i) {
                double value = i / 255.0;
                lutR[i] = static_cast<quint8>(std::clamp(
                    applyLevels(value, settings.red.inputBlack, settings.red.inputWhite,
                               settings.red.inputGamma, settings.red.outputBlack, settings.red.outputWhite) * 255.0, 0.0, 255.0));
                lutG[i] = static_cast<quint8>(std::clamp(
                    applyLevels(value, settings.green.inputBlack, settings.green.inputWhite,
                               settings.green.inputGamma, settings.green.outputBlack, settings.green.outputWhite) * 255.0, 0.0, 255.0));
                lutB[i] = static_cast<quint8>(std::clamp(
                    applyLevels(value, settings.blue.inputBlack, settings.blue.inputWhite,
                               settings.blue.inputGamma, settings.blue.outputBlack, settings.blue.outputWhite) * 255.0, 0.0, 255.0));
            }
        }
        
        lutValid = true;
    }
};

// ============================================================================
// LevelsEffect 実装
// ============================================================================

LevelsEffect::LevelsEffect() : impl_(new Impl()) {}

LevelsEffect::~LevelsEffect() {
    delete impl_;
}

void LevelsEffect::setSettings(const LevelsSettings& settings) {
    impl_->settings = settings;
    impl_->lutValid = false;
}

LevelsSettings LevelsEffect::settings() const {
    return impl_->settings;
}

QImage LevelsEffect::apply(const QImage& source) const {
    if (!impl_->lutValid) {
        impl_->buildLUT();
    }
    
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            int r = qRed(line[x]);
            int g = qGreen(line[x]);
            int b = qBlue(line[x]);
            int a = qAlpha(line[x]);
            
            line[x] = qRgba(impl_->lutR[r], impl_->lutG[g], impl_->lutB[b], a);
        }
    }
    
    return result;
}

void LevelsEffect::applyPixel(float& r, float& g, float& b) const {
    double result = applyLevels(r, 
        impl_->settings.inputBlack, impl_->settings.inputWhite,
        impl_->settings.inputGamma,
        impl_->settings.outputBlack, impl_->settings.outputWhite);
    r = g = b = static_cast<float>(result);
}

QVector<int> LevelsEffect::calculateHistogram(const QImage& image, int channel) {
    QVector<int> histogram(256, 0);
    QImage converted = image.convertToFormat(QImage::Format_ARGB32);
    
    for (int y = 0; y < converted.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(converted.scanLine(y));
        for (int x = 0; x < converted.width(); ++x) {
            int value;
            if (channel < 0) {
                // 輝度
                value = qGray(line[x]);
            } else if (channel == 0) {
                value = qRed(line[x]);
            } else if (channel == 1) {
                value = qGreen(line[x]);
            } else {
                value = qBlue(line[x]);
            }
            ++histogram[value];
        }
    }
    
    return histogram;
}

LevelsSettings LevelsEffect::calculateAutoLevels(const QVector<int>& histogram) {
    LevelsSettings settings;
    
    // 累積ヒストグラムから黒点・白点を決定
    int total = 0;
    for (int i = 0; i < 256; ++i) {
        total += histogram[i];
    }
    
    int threshold = static_cast<int>(total * 0.005); // 0.5%
    
    // 黒点
    int sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += histogram[i];
        if (sum > threshold) {
            settings.inputBlack = i;
            break;
        }
    }
    
    // 白点
    sum = 0;
    for (int i = 255; i >= 0; --i) {
        sum += histogram[i];
        if (sum > threshold) {
            settings.inputWhite = i;
            break;
        }
    }
    
    return settings;
}

double LevelsEffect::transformValue(double value) const {
    return applyLevels(value,
        impl_->settings.inputBlack, impl_->settings.inputWhite,
        impl_->settings.inputGamma,
        impl_->settings.outputBlack, impl_->settings.outputWhite);
}

// ============================================================================
// CurveChannel::Impl
// ============================================================================

class CurveChannel::Impl {
public:
    QVector<CurvePoint> points;
    
    Impl() {
        reset();
    }
    
    void reset() {
        points.clear();
        points.append(CurvePoint(0.0, 0.0));
        points.append(CurvePoint(1.0, 1.0));
    }
};

// ============================================================================
// CurveChannel 実装
// ============================================================================

CurveChannel::CurveChannel() : impl_(new Impl()) {}

CurveChannel::~CurveChannel() {
    delete impl_;
}

void CurveChannel::addPoint(const QPointF& point) {
    impl_->points.append(CurvePoint(point));
    normalize();
}

void CurveChannel::addPoint(double x, double y) {
    addPoint(QPointF(x, y));
}

void CurveChannel::removePoint(int index) {
    if (index >= 0 && index < impl_->points.size()) {
        impl_->points.removeAt(index);
    }
}

void CurveChannel::clearPoints() {
    impl_->reset();
}

void CurveChannel::movePoint(int index, const QPointF& newPosition) {
    if (index >= 0 && index < impl_->points.size()) {
        impl_->points[index].point = newPosition;
        normalize();
    }
}

int CurveChannel::pointCount() const {
    return impl_->points.size();
}

CurvePoint CurveChannel::pointAt(int index) const {
    if (index >= 0 && index < impl_->points.size()) {
        return impl_->points[index];
    }
    return CurvePoint();
}

QVector<CurvePoint> CurveChannel::points() const {
    return impl_->points;
}

double CurveChannel::evaluate(double x) const {
    if (impl_->points.isEmpty()) {
        return x;
    }
    
    // 端点の処理
    if (x <= impl_->points.first().point.x()) {
        return impl_->points.first().point.y();
    }
    if (x >= impl_->points.last().point.x()) {
        return impl_->points.last().point.y();
    }
    
    return splineInterpolate(x);
}

void CurveChannel::reset() {
    impl_->reset();
}

void CurveChannel::invert() {
    for (auto& p : impl_->points) {
        p.point.setY(1.0 - p.point.y());
    }
}

void CurveChannel::normalize() {
    // X座標でソート
    std::sort(impl_->points.begin(), impl_->points.end(),
              [](const CurvePoint& a, const CurvePoint& b) {
                  return a.point.x() < b.point.x();
              });
}

bool CurveChannel::isLinear() const {
    if (impl_->points.size() != 2) return false;
    return std::abs(impl_->points[0].point.y() - 0.0) < 0.001 &&
           std::abs(impl_->points[1].point.y() - 1.0) < 0.001;
}

double CurveChannel::splineInterpolate(double x) const {
    // 2点間の線形補間（簡易実装）
    for (int i = 0; i < impl_->points.size() - 1; ++i) {
        if (x >= impl_->points[i].point.x() && x <= impl_->points[i + 1].point.x()) {
            double t = (x - impl_->points[i].point.x()) / 
                       (impl_->points[i + 1].point.x() - impl_->points[i].point.x());
            return impl_->points[i].point.y() + 
                   t * (impl_->points[i + 1].point.y() - impl_->points[i].point.y());
        }
    }
    return x;
}

// ============================================================================
// CurvesSettings 実装
// ============================================================================

void CurvesSettings::reset() {
    master.reset();
    red.reset();
    green.reset();
    blue.reset();
    alpha.reset();
    usePerChannel = false;
}

CurvesSettings CurvesSettings::normal() {
    CurvesSettings s;
    s.reset();
    return s;
}

CurvesSettings CurvesSettings::highContrast() {
    CurvesSettings s;
    s.master.addPoint(0.0, 0.0);
    s.master.addPoint(0.25, 0.15);
    s.master.addPoint(0.75, 0.85);
    s.master.addPoint(1.0, 1.0);
    return s;
}

CurvesSettings CurvesSettings::lowContrast() {
    CurvesSettings s;
    s.master.addPoint(0.0, 0.1);
    s.master.addPoint(0.25, 0.3);
    s.master.addPoint(0.75, 0.7);
    s.master.addPoint(1.0, 0.9);
    return s;
}

CurvesSettings CurvesSettings::solarize() {
    CurvesSettings s;
    s.master.addPoint(0.0, 1.0);
    s.master.addPoint(0.5, 0.0);
    s.master.addPoint(1.0, 1.0);
    return s;
}

CurvesSettings CurvesSettings::negative() {
    CurvesSettings s;
    s.master.addPoint(0.0, 1.0);
    s.master.addPoint(1.0, 0.0);
    return s;
}

CurvesSettings CurvesSettings::posterize(int levels) {
    CurvesSettings s;
    s.master.clearPoints();
    double step = 1.0 / levels;
    for (int i = 0; i <= levels; ++i) {
        double x = i * step;
        double y = std::round(x * levels) / levels;
        s.master.addPoint(x, y);
    }
    return s;
}

// ============================================================================
// CurvesEffect::Impl
// ============================================================================

class CurvesEffect::Impl {
public:
    CurvesSettings settings;
    QVector<quint8> lutR;
    QVector<quint8> lutG;
    QVector<quint8> lutB;
    bool lutValid = false;
    
    void buildLUT() {
        lutR = CurvesEffect::generateCombinedLUT(settings.master, settings.red);
        lutG = CurvesEffect::generateCombinedLUT(settings.master, settings.green);
        lutB = CurvesEffect::generateCombinedLUT(settings.master, settings.blue);
        lutValid = true;
    }
};

// ============================================================================
// CurvesEffect 実装
// ============================================================================

CurvesEffect::CurvesEffect() : impl_(new Impl()) {}

CurvesEffect::~CurvesEffect() {
    delete impl_;
}

void CurvesEffect::setSettings(const CurvesSettings& settings) {
    impl_->settings = settings;
    impl_->lutValid = false;
}

CurvesSettings CurvesEffect::settings() const {
    return impl_->settings;
}

void CurvesEffect::setMasterCurve(const CurveChannel& curve) {
    impl_->settings.master = curve;
    impl_->lutValid = false;
}

void CurvesEffect::setRedCurve(const CurveChannel& curve) {
    impl_->settings.red = curve;
    impl_->lutValid = false;
}

void CurvesEffect::setGreenCurve(const CurveChannel& curve) {
    impl_->settings.green = curve;
    impl_->lutValid = false;
}

void CurvesEffect::setBlueCurve(const CurveChannel& curve) {
    impl_->settings.blue = curve;
    impl_->lutValid = false;
}

QImage CurvesEffect::apply(const QImage& source) const {
    if (!impl_->lutValid) {
        impl_->buildLUT();
    }
    
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            int r = qRed(line[x]);
            int g = qGreen(line[x]);
            int b = qBlue(line[x]);
            int a = qAlpha(line[x]);
            
            line[x] = qRgba(impl_->lutR[r], impl_->lutG[g], impl_->lutB[b], a);
        }
    }
    
    return result;
}

void CurvesEffect::applyPixel(float& r, float& g, float& b) const {
    r = static_cast<float>(impl_->settings.master.evaluate(r));
    g = static_cast<float>(impl_->settings.master.evaluate(g));
    b = static_cast<float>(impl_->settings.master.evaluate(b));
}

QVector<quint8> CurvesEffect::generateLUT(const CurveChannel& curve) {
    QVector<quint8> lut(256);
    for (int i = 0; i < 256; ++i) {
        double x = i / 255.0;
        double y = curve.evaluate(x);
        lut[i] = static_cast<quint8>(std::clamp(y * 255.0, 0.0, 255.0));
    }
    return lut;
}

QVector<quint8> CurvesEffect::generateCombinedLUT(
    const CurveChannel& master,
    const CurveChannel& channel) 
{
    QVector<quint8> lut(256);
    for (int i = 0; i < 256; ++i) {
        double x = i / 255.0;
        double y = channel.evaluate(x);
        y = master.evaluate(y);
        lut[i] = static_cast<quint8>(std::clamp(y * 255.0, 0.0, 255.0));
    }
    return lut;
}

// ============================================================================
// LevelsCurvesEffect::Impl
// ============================================================================

class LevelsCurvesEffect::Impl {
public:
    LevelsCurvesSettings settings;
    LevelsEffect levelsEffect;
    CurvesEffect curvesEffect;
};

// ============================================================================
// LevelsCurvesEffect 実装
// ============================================================================

LevelsCurvesEffect::LevelsCurvesEffect() : impl_(new Impl()) {}

LevelsCurvesEffect::~LevelsCurvesEffect() {
    delete impl_;
}

void LevelsCurvesEffect::setSettings(const LevelsCurvesSettings& settings) {
    impl_->settings = settings;
    impl_->levelsEffect.setSettings(settings.levels);
    impl_->curvesEffect.setSettings(settings.curves);
}

LevelsCurvesSettings LevelsCurvesEffect::settings() const {
    return impl_->settings;
}

void LevelsCurvesEffect::setLevels(const LevelsSettings& levels) {
    impl_->settings.levels = levels;
    impl_->levelsEffect.setSettings(levels);
}

void LevelsCurvesEffect::setCurves(const CurvesSettings& curves) {
    impl_->settings.curves = curves;
    impl_->curvesEffect.setSettings(curves);
}

QImage LevelsCurvesEffect::apply(const QImage& source) const {
    QImage result = source;
    
    if (impl_->settings.enableLevels && impl_->settings.enableCurves) {
        if (impl_->settings.order == LevelsCurvesSettings::Order::LevelsFirst) {
            result = impl_->levelsEffect.apply(result);
            result = impl_->curvesEffect.apply(result);
        } else {
            result = impl_->curvesEffect.apply(result);
            result = impl_->levelsEffect.apply(result);
        }
    } else if (impl_->settings.enableLevels) {
        result = impl_->levelsEffect.apply(result);
    } else if (impl_->settings.enableCurves) {
        result = impl_->curvesEffect.apply(result);
    }
    
    return result;
}

QImage LevelsCurvesEffect::applyWithLUT(const QImage& source) const {
    // 最適化版：統合LUTを使用
    return apply(source);
}

} // namespace ArtifactCore