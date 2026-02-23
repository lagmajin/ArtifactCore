module;

#include <QString>
#include <QImage>
#include <QPointF>
#include <cmath>
#include <vector>
#include <algorithm>

module ImageProcessing.SharpenDirectionalBlur;

import std;

namespace ArtifactCore {

using namespace BlurSharpenUtils;

// ============================================================================
// BlurSharpenUtils 実装
// ============================================================================

std::vector<double> BlurSharpenUtils::createGaussianKernel(double sigma, int& radius) {
    radius = static_cast<int>(std::ceil(sigma * 3.0));
    int size = 2 * radius + 1;
    std::vector<double> kernel(size);
    
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        double x = i - radius;
        kernel[i] = gaussian(x, sigma);
        sum += kernel[i];
    }
    
    // 正規化
    for (auto& k : kernel) {
        k /= sum;
    }
    
    return kernel;
}

void BlurSharpenUtils::convolveHorizontal(
    const QImage& source,
    QImage& dest,
    const std::vector<double>& kernel,
    int kernelRadius)
{
    const int width = source.width();
    const int height = source.height();
    
    for (int y = 0; y < height; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(dest.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            double r = 0, g = 0, b = 0, a = 0;
            
            for (int k = -kernelRadius; k <= kernelRadius; ++k) {
                int sx = std::clamp(x + k, 0, width - 1);
                double weight = kernel[k + kernelRadius];
                
                r += qRed(srcLine[sx]) * weight;
                g += qGreen(srcLine[sx]) * weight;
                b += qBlue(srcLine[sx]) * weight;
                a += qAlpha(srcLine[sx]) * weight;
            }
            
            dstLine[x] = qRgba(
                static_cast<int>(std::clamp(r, 0.0, 255.0)),
                static_cast<int>(std::clamp(g, 0.0, 255.0)),
                static_cast<int>(std::clamp(b, 0.0, 255.0)),
                static_cast<int>(std::clamp(a, 0.0, 255.0))
            );
        }
    }
}

void BlurSharpenUtils::convolveVertical(
    const QImage& source,
    QImage& dest,
    const std::vector<double>& kernel,
    int kernelRadius)
{
    const int width = source.width();
    const int height = source.height();
    
    for (int y = 0; y < height; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(dest.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            double r = 0, g = 0, b = 0, a = 0;
            
            for (int k = -kernelRadius; k <= kernelRadius; ++k) {
                int sy = std::clamp(y + k, 0, height - 1);
                const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(sy));
                double weight = kernel[k + kernelRadius];
                
                r += qRed(srcLine[x]) * weight;
                g += qGreen(srcLine[x]) * weight;
                b += qBlue(srcLine[x]) * weight;
                a += qAlpha(srcLine[x]) * weight;
            }
            
            dstLine[x] = qRgba(
                static_cast<int>(std::clamp(r, 0.0, 255.0)),
                static_cast<int>(std::clamp(g, 0.0, 255.0)),
                static_cast<int>(std::clamp(b, 0.0, 255.0)),
                static_cast<int>(std::clamp(a, 0.0, 255.0))
            );
        }
    }
}

QImage BlurSharpenUtils::convolve2D(
    const QImage& source,
    const std::vector<std::vector<double>>& kernel)
{
    int kHeight = kernel.size();
    int kWidth = kernel[0].size();
    int kRadiusY = kHeight / 2;
    int kRadiusX = kWidth / 2;
    
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int width = source.width();
    const int height = source.height();
    
    for (int y = 0; y < height; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            double r = 0, g = 0, b = 0;
            
            for (int ky = 0; ky < kHeight; ++ky) {
                int sy = std::clamp(y + ky - kRadiusY, 0, height - 1);
                const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(sy));
                
                for (int kx = 0; kx < kWidth; ++kx) {
                    int sx = std::clamp(x + kx - kRadiusX, 0, width - 1);
                    double weight = kernel[ky][kx];
                    
                    r += qRed(srcLine[sx]) * weight;
                    g += qGreen(srcLine[sx]) * weight;
                    b += qBlue(srcLine[sx]) * weight;
                }
            }
            
            dstLine[x] = qRgb(
                static_cast<int>(std::clamp(r, 0.0, 255.0)),
                static_cast<int>(std::clamp(g, 0.0, 255.0)),
                static_cast<int>(std::clamp(b, 0.0, 255.0))
            );
        }
    }
    
    return result;
}

// ============================================================================
// UnsharpMaskEffect::Impl
// ============================================================================

class UnsharpMaskEffect::Impl {
public:
    UnsharpMaskSettings settings;
};

// ============================================================================
// UnsharpMaskEffect 実装
// ============================================================================

UnsharpMaskEffect::UnsharpMaskEffect() : impl_(new Impl()) {}

UnsharpMaskEffect::~UnsharpMaskEffect() {
    delete impl_;
}

void UnsharpMaskEffect::setSettings(const UnsharpMaskSettings& settings) {
    impl_->settings = settings;
}

UnsharpMaskSettings UnsharpMaskEffect::settings() const {
    return impl_->settings;
}

QImage UnsharpMaskEffect::apply(const QImage& source) const {
    QImage blurred = createGaussianBlur(source, impl_->settings.radius);
    return blendImages(source, blurred, impl_->settings.amount, impl_->settings.threshold);
}

QImage UnsharpMaskEffect::applyWithOpenCV(const QImage& source) const {
    // OpenCV実装は別途opencvモジュールで提供
    return apply(source);
}

QImage UnsharpMaskEffect::createGaussianBlur(const QImage& source, double radius) const {
    if (radius <= 0.0) {
        return source;
    }
    
    int kernelRadius;
    auto kernel = createGaussianKernel(radius, kernelRadius);
    
    QImage temp(source.size(), QImage::Format_ARGB32);
    QImage result(source.size(), QImage::Format_ARGB32);
    
    convolveHorizontal(source, temp, kernel, kernelRadius);
    convolveVertical(temp, result, kernel, kernelRadius);
    
    return result;
}

QImage UnsharpMaskEffect::blendImages(
    const QImage& original,
    const QImage& blurred,
    double amount,
    double threshold) const
{
    QImage result = original.convertToFormat(QImage::Format_ARGB32);
    double blendFactor = amount / 100.0;
    
    const int width = original.width();
    const int height = original.height();
    
    for (int y = 0; y < height; ++y) {
        const QRgb* origLine = reinterpret_cast<const QRgb*>(original.scanLine(y));
        const QRgb* blurLine = reinterpret_cast<const QRgb*>(blurred.scanLine(y));
        QRgb* resultLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            int r = qRed(origLine[x]);
            int g = qGreen(origLine[x]);
            int b = qBlue(origLine[x]);
            int a = qAlpha(origLine[x]);
            
            int blurR = qRed(blurLine[x]);
            int blurG = qGreen(blurLine[x]);
            int blurB = qBlue(blurLine[x]);
            
            // 差分を計算
            int diffR = r - blurR;
            int diffG = g - blurG;
            int diffB = b - blurB;
            
            // 閾値チェック
            int diff = std::abs(diffR) + std::abs(diffG) + std::abs(diffB);
            if (diff / 3.0 < threshold) {
                continue;
            }
            
            // アンシャープマスク適用
            r = static_cast<int>(std::clamp(r + diffR * blendFactor, 0.0, 255.0));
            g = static_cast<int>(std::clamp(g + diffG * blendFactor, 0.0, 255.0));
            b = static_cast<int>(std::clamp(b + diffB * blendFactor, 0.0, 255.0));
            
            resultLine[x] = qRgba(r, g, b, a);
        }
    }
    
    return result;
}

// ============================================================================
// AdvancedSharpenEffect::Impl
// ============================================================================

class AdvancedSharpenEffect::Impl {
public:
    AdvancedSharpenEffect::Algorithm algorithm = AdvancedSharpenEffect::Algorithm::UnsharpMask;
    double radius = 1.0;
    double amount = 50.0;
    double threshold = 0.0;
};

// ============================================================================
// AdvancedSharpenEffect 実装
// ============================================================================

AdvancedSharpenEffect::AdvancedSharpenEffect() : impl_(new Impl()) {}

AdvancedSharpenEffect::~AdvancedSharpenEffect() {
    delete impl_;
}

void AdvancedSharpenEffect::setAlgorithm(Algorithm algo) {
    impl_->algorithm = algo;
}

AdvancedSharpenEffect::Algorithm AdvancedSharpenEffect::algorithm() const {
    return impl_->algorithm;
}

void AdvancedSharpenEffect::setRadius(double radius) {
    impl_->radius = radius;
}

void AdvancedSharpenEffect::setAmount(double amount) {
    impl_->amount = amount;
}

void AdvancedSharpenEffect::setThreshold(double threshold) {
    impl_->threshold = threshold;
}

QImage AdvancedSharpenEffect::apply(const QImage& source) const {
    switch (impl_->algorithm) {
        case Algorithm::UnsharpMask: {
            UnsharpMaskEffect effect;
            UnsharpMaskSettings s;
            s.radius = impl_->radius;
            s.amount = impl_->amount;
            s.threshold = impl_->threshold;
            effect.setSettings(s);
            return effect.apply(source);
        }
        case Algorithm::HighPass: {
            // ハイパスフィルタ実装
            UnsharpMaskEffect effect;
            UnsharpMaskSettings s;
            s.radius = impl_->radius * 2;
            s.amount = impl_->amount * 0.5;
            effect.setSettings(s);
            return effect.apply(source);
        }
        case Algorithm::Laplacian: {
            // ラプラシアンシャープン
            std::vector<std::vector<double>> kernel = {
                { 0, -1,  0},
                {-1,  5, -1},
                { 0, -1,  0}
            };
            return convolve2D(source, kernel);
        }
        case Algorithm::SmartSharpen: {
            // スマートシャープン（簡易実装）
            UnsharpMaskEffect effect;
            UnsharpMaskSettings s;
            s.radius = impl_->radius;
            s.amount = impl_->amount;
            s.threshold = impl_->threshold + 5.0;  // エッジ保護強化
            effect.setSettings(s);
            return effect.apply(source);
        }
    }
    return source;
}

// ============================================================================
// DirectionalBlurEffect::Impl
// ============================================================================

class DirectionalBlurEffect::Impl {
public:
    DirectionalBlurSettings settings;
};

// ============================================================================
// DirectionalBlurEffect 実装
// ============================================================================

DirectionalBlurEffect::DirectionalBlurEffect() : impl_(new Impl()) {}

DirectionalBlurEffect::~DirectionalBlurEffect() {
    delete impl_;
}

void DirectionalBlurEffect::setSettings(const DirectionalBlurSettings& settings) {
    impl_->settings = settings;
}

DirectionalBlurSettings DirectionalBlurEffect::settings() const {
    return impl_->settings;
}

void DirectionalBlurEffect::setAngle(double degrees) {
    impl_->settings.angle = degrees;
}

void DirectionalBlurEffect::setRadius(double pixels) {
    impl_->settings.radius = pixels;
}

void DirectionalBlurEffect::setIterations(int count) {
    impl_->settings.iterations = count;
}

QImage DirectionalBlurEffect::apply(const QImage& source) const {
    double radians = degreesToRadians(impl_->settings.angle);
    double dx = std::cos(radians);
    double dy = std::sin(radians);
    
    return blurInDirection(source, dx, dy, impl_->settings.iterations);
}

QImage DirectionalBlurEffect::applyWithOpenCV(const QImage& source) const {
    return apply(source);
}

QImage DirectionalBlurEffect::applyAsMotionBlur(const QImage& source, double angle, double distance) const {
    double radians = degreesToRadians(angle);
    double dx = std::cos(radians);
    double dy = std::sin(radians);
    
    int steps = static_cast<int>(distance / 2.0) + 1;
    
    return blurInDirection(source, dx, dy, steps);
}

QImage DirectionalBlurEffect::blurInDirection(
    const QImage& source,
    double dx,
    double dy,
    int steps) const
{
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    const int width = source.width();
    const int height = source.height();
    const double radius = impl_->settings.radius;
    const double stepSize = radius / steps;
    
    for (int y = 0; y < height; ++y) {
        QRgb* resultLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            double sumR = qRed(resultLine[x]);
            double sumG = qGreen(resultLine[x]);
            double sumB = qBlue(resultLine[x]);
            double sumA = qAlpha(resultLine[x]);
            int count = 1;
            
            // 正方向
            for (int s = 1; s <= steps; ++s) {
                double offset = s * stepSize;
                int sx = static_cast<int>(x + dx * offset);
                int sy = static_cast<int>(y + dy * offset);
                
                if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                    const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(sy));
                    sumR += qRed(srcLine[sx]);
                    sumG += qGreen(srcLine[sx]);
                    sumB += qBlue(srcLine[sx]);
                    sumA += qAlpha(srcLine[sx]);
                    ++count;
                }
            }
            
            // 負方向
            for (int s = 1; s <= steps; ++s) {
                double offset = s * stepSize;
                int sx = static_cast<int>(x - dx * offset);
                int sy = static_cast<int>(y - dy * offset);
                
                if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                    const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(sy));
                    sumR += qRed(srcLine[sx]);
                    sumG += qGreen(srcLine[sx]);
                    sumB += qBlue(srcLine[sx]);
                    sumA += qAlpha(srcLine[sx]);
                    ++count;
                }
            }
            
            resultLine[x] = qRgba(
                static_cast<int>(sumR / count),
                static_cast<int>(sumG / count),
                static_cast<int>(sumB / count),
                static_cast<int>(sumA / count)
            );
        }
    }
    
    return result;
}

// ============================================================================
// RadialBlurEffect::Impl
// ============================================================================

class RadialBlurEffect::Impl {
public:
    RadialBlurSettings settings;
};

// ============================================================================
// RadialBlurEffect 実装
// ============================================================================

RadialBlurEffect::RadialBlurEffect() : impl_(new Impl()) {}

RadialBlurEffect::~RadialBlurEffect() {
    delete impl_;
}

void RadialBlurEffect::setSettings(const RadialBlurSettings& settings) {
    impl_->settings = settings;
}

RadialBlurSettings RadialBlurEffect::settings() const {
    return impl_->settings;
}

void RadialBlurEffect::setCenter(const QPointF& center) {
    impl_->settings.center = center;
}

void RadialBlurEffect::setCenter(double x, double y) {
    impl_->settings.center = QPointF(x, y);
}

void RadialBlurEffect::setType(RadialBlurType type) {
    impl_->settings.type = type;
}

QImage RadialBlurEffect::apply(const QImage& source) const {
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    const int width = source.width();
    const int height = source.height();
    
    // 中心点をピクセル座標に変換
    double cx = impl_->settings.center.x() * width;
    double cy = impl_->settings.center.y() * height;
    
    const int quality = impl_->settings.quality;
    const double angleStep = impl_->settings.angle / quality;
    const double zoomFactor = impl_->settings.zoom / quality;
    
    for (int y = 0; y < height; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.scanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            double sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            int count = 0;
            
            for (int q = -quality; q <= quality; ++q) {
                double dx = x - cx;
                double dy = y - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                
                // 回転
                double angle = q * angleStep * M_PI / 180.0;
                double cosA = std::cos(angle);
                double sinA = std::sin(angle);
                
                double rx = dx * cosA - dy * sinA;
                double ry = dx * sinA + dy * cosA;
                
                // ズーム
                double zoom = 1.0 + q * zoomFactor / 100.0;
                rx *= zoom;
                ry *= zoom;
                
                int sx = static_cast<int>(cx + rx);
                int sy = static_cast<int>(cy + ry);
                
                if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
                    const QRgb* sampleLine = reinterpret_cast<const QRgb*>(source.scanLine(sy));
                    sumR += qRed(sampleLine[sx]);
                    sumG += qGreen(sampleLine[sx]);
                    sumB += qBlue(sampleLine[sx]);
                    sumA += qAlpha(sampleLine[sx]);
                    ++count;
                }
            }
            
            if (count > 0) {
                dstLine[x] = qRgba(
                    static_cast<int>(sumR / count),
                    static_cast<int>(sumG / count),
                    static_cast<int>(sumB / count),
                    static_cast<int>(sumA / count)
                );
            } else {
                dstLine[x] = srcLine[x];
            }
        }
    }
    
    return result;
}

// ============================================================================
// BlurEffect::Impl
// ============================================================================

class BlurEffect::Impl {
public:
    BlurEffectSettings settings;
};

// ============================================================================
// BlurEffect 実装
// ============================================================================

BlurEffect::BlurEffect() : impl_(new Impl()) {}

BlurEffect::~BlurEffect() {
    delete impl_;
}

void BlurEffect::setSettings(const BlurEffectSettings& settings) {
    impl_->settings = settings;
}

BlurEffectSettings BlurEffect::settings() const {
    return impl_->settings;
}

void BlurEffect::setType(BlurEffectSettings::Type type) {
    impl_->settings.type = type;
}

QImage BlurEffect::apply(const QImage& source) const {
    switch (impl_->settings.type) {
        case BlurEffectSettings::Type::Gaussian: {
            int kernelRadius;
            auto kernel = createGaussianKernel(impl_->settings.radius, kernelRadius);
            QImage temp(source.size(), QImage::Format_ARGB32);
            QImage result(source.size(), QImage::Format_ARGB32);
            convolveHorizontal(source, temp, kernel, kernelRadius);
            convolveVertical(temp, result, kernel, kernelRadius);
            return result;
        }
        case BlurEffectSettings::Type::Directional: {
            DirectionalBlurEffect effect;
            DirectionalBlurSettings s;
            s.angle = impl_->settings.angle;
            s.radius = impl_->settings.radius;
            s.iterations = impl_->settings.quality;
            effect.setSettings(s);
            return effect.apply(source);
        }
        case BlurEffectSettings::Type::Radial: {
            RadialBlurEffect effect;
            RadialBlurSettings s;
            s.center = impl_->settings.center;
            s.quality = impl_->settings.quality;
            s.zoom = impl_->settings.zoom;
            effect.setSettings(s);
            return effect.apply(source);
        }
        case BlurEffectSettings::Type::Motion: {
            DirectionalBlurEffect effect;
            return effect.applyAsMotionBlur(source, impl_->settings.angle, impl_->settings.radius);
        }
        case BlurEffectSettings::Type::Box: {
            // ボックスブラー
            int r = static_cast<int>(impl_->settings.radius);
            std::vector<double> kernel(2 * r + 1, 1.0 / (2 * r + 1));
            QImage temp(source.size(), QImage::Format_ARGB32);
            QImage result(source.size(), QImage::Format_ARGB32);
            convolveHorizontal(source, temp, kernel, r);
            convolveVertical(temp, result, kernel, r);
            return result;
        }
        case BlurEffectSettings::Type::Bilateral: {
            // バイラテラルフィルタは簡易実装
            // 実際はOpenCV等を使用
            return source;
        }
    }
    return source;
}

} // namespace ArtifactCore