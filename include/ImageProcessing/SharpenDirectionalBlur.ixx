module;

#include <QString>
#include <QImage>
#include <QPointF>

export module ImageProcessing.SharpenDirectionalBlur;

import std;

export namespace ArtifactCore {

// ============================================================================
// Unsharp Mask（アンシャープマスク・シャープ化）
// ============================================================================

/// アンシャープマスク設定
struct UnsharpMaskSettings {
    double radius = 1.0;        ///< ブラ半径（ピクセル）
    double amount = 50.0;       ///< 適用量（%）
    double threshold = 0.0;     ///< 閾値（0-255）
    
    /// チャンネル選択
    enum class ChannelMode {
        All,        ///< 全チャンネル
        Luminance,  ///< 輝度のみ
        RGB         ///< RGB各チャンネル
    };
    ChannelMode channelMode = ChannelMode::All;
    
    /// プリセット
    static UnsharpMaskSettings normal() { 
        return UnsharpMaskSettings{}; 
    }
    
    static UnsharpMaskSettings strong() {
        UnsharpMaskSettings s;
        s.radius = 2.0;
        s.amount = 100.0;
        return s;
    }
    
    static UnsharpMaskSettings subtle() {
        UnsharpMaskSettings s;
        s.radius = 0.5;
        s.amount = 25.0;
        return s;
    }
    
    static UnsharpMaskSettings edgeEnhance() {
        UnsharpMaskSettings s;
        s.radius = 1.0;
        s.amount = 75.0;
        s.threshold = 10.0;
        return s;
    }
};

/// アンシャープマスクエフェクト
class UnsharpMaskEffect {
public:
    UnsharpMaskEffect();
    ~UnsharpMaskEffect();
    
    /// 設定
    void setSettings(const UnsharpMaskSettings& settings);
    UnsharpMaskSettings settings() const;
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// OpenCVを使用した高速実装
    QImage applyWithOpenCV(const QImage& source) const;

private:
    class Impl;
    Impl* impl_;
    
    /// ガウシアンブラー生成
    QImage createGaussianBlur(const QImage& source, double radius) const;
    
    /// ブレンド
    QImage blendImages(const QImage& original, const QImage& blurred, double amount, double threshold) const;
};

/// 高性能シャープ化（複数アルゴリズム対応）
class AdvancedSharpenEffect {
public:
    /// シャープ化アルゴリズム
    enum class Algorithm {
        UnsharpMask,    ///< アンシャープマスク
        HighPass,       ///< ハイパスフィルタ
        Laplacian,      ///< ラプラシアン
        SmartSharpen    ///< スマートシャープン（エッジ検出付き）
    };
    
    AdvancedSharpenEffect();
    ~AdvancedSharpenEffect();
    
    /// アルゴリズム選択
    void setAlgorithm(Algorithm algo);
    Algorithm algorithm() const;
    
    /// 設定
    void setRadius(double radius);
    void setAmount(double amount);
    void setThreshold(double threshold);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;

private:
    class Impl;
    Impl* impl_;
};

// ============================================================================
// Directional Blur（方向ブラー）
// ============================================================================

/// 方向ブラー設定
struct DirectionalBlurSettings {
    double angle = 0.0;         ///< ブラー方向（度）
    double radius = 10.0;       ///< ブラー半径（ピクセル）
    int iterations = 3;         ///< 反復回数（品質）
    
    /// エッジ処理
    enum class EdgeMode {
        Transparent,    ///< 透明
        Wrap,           ///< ラップ
        Reflect,        ///< 反射
        Extend          ///< 拡張
    };
    EdgeMode edgeMode = EdgeMode::Transparent;
    
    /// プリセット
    static DirectionalBlurSettings normal() {
        return DirectionalBlurSettings{};
    }
    
    static DirectionalBlurSettings motionBlur() {
        DirectionalBlurSettings s;
        s.angle = 0.0;
        s.radius = 20.0;
        s.iterations = 5;
        return s;
    }
    
    static DirectionalBlurSettings horizontal() {
        DirectionalBlurSettings s;
        s.angle = 0.0;
        s.radius = 15.0;
        return s;
    }
    
    static DirectionalBlurSettings vertical() {
        DirectionalBlurSettings s;
        s.angle = 90.0;
        s.radius = 15.0;
        return s;
    }
    
    static DirectionalBlurSettings diagonal() {
        DirectionalBlurSettings s;
        s.angle = 45.0;
        s.radius = 12.0;
        return s;
    }
};

/// 方向ブラーエフェクト
class DirectionalBlurEffect {
public:
    DirectionalBlurEffect();
    ~DirectionalBlurEffect();
    
    /// 設定
    void setSettings(const DirectionalBlurSettings& settings);
    DirectionalBlurSettings settings() const;
    
    /// 個別設定
    void setAngle(double degrees);
    void setRadius(double pixels);
    void setIterations(int count);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// OpenCVを使用した高速実装
    QImage applyWithOpenCV(const QImage& source) const;
    
    /// モーションブラーとして適用（速度最適化）
    QImage applyAsMotionBlur(const QImage& source, double angle, double distance) const;

private:
    class Impl;
    Impl* impl_;
    
    /// 単一方向ブラー
    QImage blurInDirection(const QImage& source, double dx, double dy, int steps) const;
};

// ============================================================================
// Radial Blur（放射ブラー）
// ============================================================================

/// 放射ブラータイプ
enum class RadialBlurType {
    Spin,       ///< 回転ブラー
    Zoom,       ///< ズームブラー
    Both        ///< 両方
};

/// 放射ブラー設定
struct RadialBlurSettings {
    QPointF center = QPointF(0.5, 0.5);  ///< 中心点（0-1正規化）
    double angle = 10.0;                  ///< 回転角度
    double zoom = 0.1;                    ///< ズーム量
    int quality = 3;                      ///< 品質（反復回数）
    RadialBlurType type = RadialBlurType::Spin;
};

/// 放射ブラーエフェクト
class RadialBlurEffect {
public:
    RadialBlurEffect();
    ~RadialBlurEffect();
    
    /// 設定
    void setSettings(const RadialBlurSettings& settings);
    RadialBlurSettings settings() const;
    
    /// 中心点設定（ピクセル座標）
    void setCenter(const QPointF& center);
    void setCenter(double x, double y);
    
    /// タイプ設定
    void setType(RadialBlurType type);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;

private:
    class Impl;
    Impl* impl_;
};

// ============================================================================
// 統合ブラーエフェクト
// ============================================================================

/// ブラータイプ統合
struct BlurEffectSettings {
    enum class Type {
        Gaussian,       ///< ガウシアンブラー
        Directional,    ///< 方向ブラー
        Radial,         ///< 放射ブラー
        Motion,         ///< モーションブラー
        Box,            ///< ボックスブラー
        Bilateral       ///< バイラテラルフィルタ
    };
    
    Type type = Type::Gaussian;
    
    /// 共通パラメータ
    double radius = 5.0;
    double angle = 0.0;
    QPointF center = QPointF(0.5, 0.5);
    int quality = 3;
    
    /// タイプ別パラメータ
    double zoom = 0.0;           ///< Radial用
    double spatialSigma = 10.0;  ///< Bilateral用
    double colorSigma = 30.0;    ///< Bilateral用
};

/// 統合ブラーエフェクト
class BlurEffect {
public:
    BlurEffect();
    ~BlurEffect();
    
    /// 設定
    void setSettings(const BlurEffectSettings& settings);
    BlurEffectSettings settings() const;
    
    /// タイプ設定
    void setType(BlurEffectSettings::Type type);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;

private:
    class Impl;
    Impl* impl_;
};

// ============================================================================
// ユーティリティ
// ============================================================================

namespace BlurSharpenUtils {
    /// 度からラジアンへ変換
    inline double degreesToRadians(double degrees) {
        return degrees * M_PI / 180.0;
    }
    
    /// ラジアンから度へ変換
    inline double radiansToDegrees(double radians) {
        return radians * 180.0 / M_PI;
    }
    
    /// ガウス関数
    inline double gaussian(double x, double sigma) {
        return std::exp(-(x * x) / (2.0 * sigma * sigma));
    }
    
    /// ガウスカーネル生成
    std::vector<double> createGaussianKernel(double sigma, int& radius);
    
    /// 1D畳み込み（水平）
    void convolveHorizontal(
        const QImage& source,
        QImage& dest,
        const std::vector<double>& kernel,
        int kernelRadius
    );
    
    /// 1D畳み込み（垂直）
    void convolveVertical(
        const QImage& source,
        QImage& dest,
        const std::vector<double>& kernel,
        int kernelRadius
    );
    
    /// 2D畳み込み
    QImage convolve2D(
        const QImage& source,
        const std::vector<std::vector<double>>& kernel
    );
}

} // namespace ArtifactCore