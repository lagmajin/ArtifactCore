module;

#include <QString>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <vector>

export module ImageProcessing.ColorTransform.LevelsCurves;

import std;

export namespace ArtifactCore {

// ============================================================================
// Levels（レベル補正）
// ============================================================================

/// レベル補正設定
struct LevelsSettings {
    // 入力レベル
    double inputBlack = 0.0;     ///< 入力黒レベル (0-255)
    double inputWhite = 255.0;   ///< 入力白レベル (0-255)
    double inputGamma = 1.0;     ///< 入力ガンマ (0.01-10.0)
    
    // 出力レベル
    double outputBlack = 0.0;    ///< 出力黒レベル (0-255)
    double outputWhite = 255.0;  ///< 出力白レベル (0-255)
    
    // チャンネルごとの設定（オプション）
    bool perChannel = false;     ///< チャンネルごとの調整を有効
    LevelsSettings red;
    LevelsSettings green;
    LevelsSettings blue;
    
    /// リセット
    void reset() {
        inputBlack = 0.0;
        inputWhite = 255.0;
        inputGamma = 1.0;
        outputBlack = 0.0;
        outputWhite = 255.0;
        perChannel = false;
    }
    
    /// 自動レベル（ヒストグラムベース）
    static LevelsSettings autoLevels(const QImage& image);
    
    /// プリセット
    static LevelsSettings normal() { return LevelsSettings{}; }
    static LevelsSettings highContrast();
    static LevelsSettings lowContrast();
    static LevelsSettings brighten();
    static LevelsSettings darken();
};

/// レベル補正エフェクト
class LevelsEffect {
public:
    LevelsEffect();
    ~LevelsEffect();
    
    /// 設定
    void setSettings(const LevelsSettings& settings);
    LevelsSettings settings() const;
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// 単一ピクセルに適用（0-1範囲）
    void applyPixel(float& r, float& g, float& b) const;
    
    /// ヒストグラム計算
    static QVector<int> calculateHistogram(const QImage& image, int channel = -1);
    
    /// ヒストグラムから自動設定を計算
    static LevelsSettings calculateAutoLevels(const QVector<int>& histogram);

private:
    class Impl;
    Impl* impl_;
    
    /// 値をトランスフォーム
    double transformValue(double value) const;
};

// ============================================================================
// Curves（カーブ調整）
// ============================================================================

/// カーブポイント
struct CurvePoint {
    QPointF point;       ///< 座標 (0-1, 0-1)
    bool selected = false;
    
    CurvePoint() = default;
    CurvePoint(double x, double y) : point(x, y) {}
    CurvePoint(const QPointF& p) : point(p) {}
};

/// 単一チャンネルのカーブ
class CurveChannel {
public:
    CurveChannel();
    ~CurveChannel();
    
    /// ポイント追加
    void addPoint(const QPointF& point);
    void addPoint(double x, double y);
    
    /// ポイント削除
    void removePoint(int index);
    
    /// 全ポイント削除
    void clearPoints();
    
    /// ポイント移動
    void movePoint(int index, const QPointF& newPosition);
    
    /// ポイント数
    int pointCount() const;
    
    /// ポイント取得
    CurvePoint pointAt(int index) const;
    QVector<CurvePoint> points() const;
    
    /// 値を評価（0-1入力 → 0-1出力）
    double evaluate(double x) const;
    
    /// 線形カーブにリセット
    void reset();
    
    /// 反転
    void invert();
    
    /// ポイントを正規化（x座標でソート）
    void normalize();
    
    /// カーブが線形かどうか
    bool isLinear() const;

private:
    class Impl;
    Impl* impl_;
    
    /// スプライン補間
    double splineInterpolate(double x) const;
};

/// カーブ調整設定
struct CurvesSettings {
    CurveChannel master;    ///< マスターカーブ
    CurveChannel red;       ///< Rチャンネル
    CurveChannel green;     ///< Gチャンネル
    CurveChannel blue;      ///< Bチャンネル
    CurveChannel alpha;     ///< アルファチャンネル
    
    /// 個別チャンネル調整を有効
    bool usePerChannel = false;
    
    /// リセット
    void reset();
    
    /// プリセット
    static CurvesSettings normal();
    static CurvesSettings highContrast();
    static CurvesSettings lowContrast();
    static CurvesSettings solarize();
    static CurvesSettings negative();
    static CurvesSettings posterize(int levels = 4);
};

/// カーブ調整エフェクト
class CurvesEffect {
public:
    CurvesEffect();
    ~CurvesEffect();
    
    /// 設定
    void setSettings(const CurvesSettings& settings);
    CurvesSettings settings() const;
    
    /// 個別チャンネル設定
    void setMasterCurve(const CurveChannel& curve);
    void setRedCurve(const CurveChannel& curve);
    void setGreenCurve(const CurveChannel& curve);
    void setBlueCurve(const CurveChannel& curve);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// 単一ピクセルに適用（0-1範囲）
    void applyPixel(float& r, float& g, float& b) const;
    
    /// ルックアップテーブル生成（高速化用）
    static QVector<quint8> generateLUT(const CurveChannel& curve);
    
    /// 複数カーブを合成したLUT生成
    static QVector<quint8> generateCombinedLUT(
        const CurveChannel& master,
        const CurveChannel& channel);

private:
    class Impl;
    Impl* impl_;
};

// ============================================================================
// Levels & Curves 統合エフェクト
// ============================================================================

/// レベル＆カーブ統合設定
struct LevelsCurvesSettings {
    LevelsSettings levels;
    CurvesSettings curves;
    bool enableLevels = true;
    bool enableCurves = true;
    
    /// 処理順序
    enum class Order {
        LevelsFirst,    ///< レベル→カーブ
        CurvesFirst     ///< カーブ→レベル
    };
    Order order = Order::LevelsFirst;
};

/// レベル＆カーブ統合エフェクト
class LevelsCurvesEffect {
public:
    LevelsCurvesEffect();
    ~LevelsCurvesEffect();
    
    /// 設定
    void setSettings(const LevelsCurvesSettings& settings);
    LevelsCurvesSettings settings() const;
    
    /// 個別設定
    void setLevels(const LevelsSettings& levels);
    void setCurves(const CurvesSettings& curves);
    
    /// 画像に適用
    QImage apply(const QImage& source) const;
    
    /// 最適化されたLUTを生成して適用
    QImage applyWithLUT(const QImage& source) const;

private:
    class Impl;
    Impl* impl_;
};

// ============================================================================
// ユーティリティ
// ============================================================================

namespace ColorAdjustmentUtils {
    /// 値をガンマ補正
    inline double applyGamma(double value, double gamma) {
        return std::pow(std::clamp(value, 0.0, 1.0), 1.0 / gamma);
    }
    
    /// 値をレベル補正
    inline double applyLevels(double value, double inBlack, double inWhite, 
                               double gamma, double outBlack, double outWhite) {
        // 入力レベル適用
        double normalized = (value * 255.0 - inBlack) / (inWhite - inBlack);
        normalized = std::clamp(normalized, 0.0, 1.0);
        
        // ガンマ適用
        normalized = applyGamma(normalized, gamma);
        
        // 出力レベル適用
        return (normalized * (outWhite - outBlack) + outBlack) / 255.0;
    }
    
    /// RGB値を0-1に正規化
    inline void normalizeRGB(quint8 r, quint8 g, quint8 b, float& nr, float& ng, float& nb) {
        nr = r / 255.0f;
        ng = g / 255.0f;
        nb = b / 255.0f;
    }
    
    /// RGB値を0-255に変換
    inline void denormalizeRGB(float nr, float ng, float nb, quint8& r, quint8& g, quint8& b) {
        r = static_cast<quint8>(std::clamp(nr * 255.0f, 0.0f, 255.0f));
        g = static_cast<quint8>(std::clamp(ng * 255.0f, 0.0f, 255.0f));
        b = static_cast<quint8>(std::clamp(nb * 255.0f, 0.0f, 255.0f));
    }
}

} // namespace ArtifactCore