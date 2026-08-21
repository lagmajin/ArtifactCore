module;
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <variant>
#include <optional>
#include <limits>
#include <cmath>
#include <algorithm>
#include <functional>
#include <QRectF>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QVector3D>

export module Artifact.Grid.System;

import Particle;
import Color.Float;
import Container.NamedVector;
// PrimitiveRenderer2D is declared in Artifact; forward-declare to avoid
// requiring the Artifact.Render.PrimitiveRenderer2D module at this header
// level. The full module is used in implementation files where needed.
export namespace Artifact { class PrimitiveRenderer2D; }

using namespace ArtifactCore;

export namespace Artifact::Grid {

/**
 * @brief 単位種別
 */
enum class Unit {
    Pixels,      ///< ピクセル（ビューポート相対）
    Centimeters, ///< 印刷・実世界単位
    Inches,      ///< インチ
    Points,      ///> ポイント（1/72インチ）
    Millimeters  ///> ミリメートル
};

/**
 * @brief DPI設定（単位→ピクセル変換用）
 */
struct DpiSettings {
    float horizontalDpi = 96.0f;
    float verticalDpi   = 96.0f;
    float factor() const {
        return std::isfinite(horizontalDpi) && horizontalDpi > 0.0f
            ? horizontalDpi : 96.0f;
    }
    float verticalFactor() const {
        return std::isfinite(verticalDpi) && verticalDpi > 0.0f
            ? verticalDpi : 96.0f;
    }
};

/**
 * @brief グリッド線のスタイル
 */
enum class GridLineStyle {
    Solid,      ///< 実線
    Dash,       ///< 点線
    Dot,        ///< ドット
    DashDot,    ///< 一点鎖線
    DashDotDot  ///< 二点鎖線
};

/**
 * @brief グリッド線のプロパティ
 */
struct GridLineStyleDef {
    GridLineStyle style = GridLineStyle::Solid;
    float thickness = 1.0f;           ///< 線の太さ（ピクセル）
    std::array<float, 4> dashPattern; ///< 実・仮長さの配列（styleがDash系のみ）
};

/**
 * @brief グリッド設定
 *
 * Mayaのグリッドシステムに相当：
 * - 主グリッド間隔（major interval）と細分（subdivisions）
 * - カメラの追従・範囲制御
 * - カスタム単位とDPI連動
 */
struct GridSettings {
    // --- 間隔・分割 ---
    float majorInterval = 100.0f;     ///< 主グリッド間隔（単位座標）
    int   subdivisions  = 4;           ///< 主グリッドを何等分か（1=細グリッドなし、4=4分割）
    bool  snapToGrid    = true;       ///< スナップ有効

    // --- 表示範囲（カメラ視界）---
    float visibleRangeStart = -10000.0f; ///< 表示開始（単位）
    float visibleRangeEnd   = 10000.0f;  ///< 表示終了（単位）

    // --- カラー・ ---
    ArtifactCore::FloatColor majorColor = ArtifactCore::FloatColor(0.45f, 0.45f, 0.45f, 0.8f);  ///< 主グリッド色
    ArtifactCore::FloatColor minorColor = ArtifactCore::FloatColor(0.25f, 0.25f, 0.25f, 0.4f);  ///< 細グリッド色
    ArtifactCore::FloatColor axisColor  = ArtifactCore::FloatColor(0.9f, 0.3f, 0.3f, 0.9f);    ///> 原点軸強調色
    ArtifactCore::FloatColor backgroundColor = ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f); ///< グリッド背景

    // --- 線種 ---
    GridLineStyleDef majorStyle;
    GridLineStyleDef minorStyle;
    GridLineStyleDef axisStyle;

    // --- 単位 ---
    Unit unit = Unit::Pixels;
    DpiSettings dpi;

    // --- オプション ---
    bool showMajor   = true;
    bool showMinor   = true;
    bool showAxis    = true;    ///< 原点（X=0,Y=0）線を強調
    bool showNumbers = false;   ///< グリッド数値を描画（後で拡張）

    // --- キャッシュ・計算結果（readonly）---
    mutable float cachedMajorPixelInterval = 0.0f;
    mutable float cachedMinorPixelInterval = 0.0f;
    mutable bool  cacheValid = false;
};

/**
 * @brief ビューポート・変換情報
 */
struct GridViewTransform {
    QTransform canvasToViewport;   ///< canvas座標→viewport座標
    QRectF      visibleCanvasRect; ///< 現在の表示canvas領域（ワールド座標）
    float       zoom = 1.0f;       ///< ズーム率
    QPointF     pan  = {0.0f, 0.0f}; ///< パン量

    // キャッシュ
    mutable float cachedPixelsPerUnit = 1.0f;
    mutable bool  cacheDirty = true;
};

/** @brief 複数グリッド管理で使用するグリッド種別。 */
enum class GridType : std::uint8_t {
    Rectangular, Polar, Isometric, Radial, Perspective, Custom
};

enum class GridPlane : std::uint8_t {
    XY, XZ, YZ, View
};

struct GridPlaneTransform {
    GridPlane plane = GridPlane::XY;
    QVector3D origin{0.0f, 0.0f, 0.0f};
    QVector3D xAxis{1.0f, 0.0f, 0.0f};
    QVector3D yAxis{0.0f, 1.0f, 0.0f};
};

/** @brief ズームに応じた主間隔の自動調整設定。 */
struct GridAutoStepConfig {
    struct FadeLevel {
        float zoomMax = 1.0f;
        int majorStepMultiplier = 1;
        float alpha = 1.0f;
    };
    bool enabled = false;
    float targetViewportInterval = 100.0f;
    int minSubdivisions = 1;
    int maxSubdivisions = 10;
    bool useNiceNumbers = true;
    bool useViewFade = false;
    std::vector<FadeLevel> fadeLevels;
};

struct GridLine;

struct GridRegion {
    QString name;
    QRectF canvasRect;
    std::optional<GridSettings> overrideSettings;
    bool enabled = true;
};

/** @brief 複数グリッドの一つを記述する定義。 */
struct GridDescriptor {
    QString name;
    GridType type = GridType::Rectangular;
    GridPlaneTransform plane;
    GridSettings baseSettings;
    GridAutoStepConfig autoStep;
    std::vector<GridRegion> regions;
    QPointF origin;
    QPointF vanishingPoint;
    float horizonY = 0.0f;
    float radialInterval = 100.0f;
    int angularSubdivisions = 12;
    int perspectiveLineCount = 12;
    bool visible = true;
    int zOrder = 0;
    float globalAlpha = 1.0f;
    bool showLabels = false;
    float labelFontSize = 9.0f;
    bool snapEnabled = true;
    float snapPriority = 1.0f;
    std::function<std::vector<GridLine>(const GridDescriptor&,
                                        const GridViewTransform&)> customGenerator;
};

/** @brief 描画用に始点・終点へ展開されたグリッド線。 */
struct GridLine {
    QPointF start;
    QPointF end;
    ArtifactCore::FloatColor color;
    float thickness = 1.0f;
    GridLineStyle style = GridLineStyle::Solid;
    bool isMajor = true;
    std::optional<QString> label;
};

/** @brief 3D viewport ground-grid line on the XZ plane. */
struct GroundGridLine {
    QVector3D start;
    QVector3D end;
    ArtifactCore::FloatColor color;
    float thickness = 1.0f;
    bool isMajor = true;
};

struct GroundGridSettings {
    float majorInterval = 1.0f;
    int subdivisions = 10;
    float extent = 100.0f;
    float fadeStart = 60.0f;
    float fadeEnd = 100.0f;
    ArtifactCore::FloatColor majorColor =
        ArtifactCore::FloatColor(0.35f, 0.42f, 0.50f, 0.75f);
    ArtifactCore::FloatColor minorColor =
        ArtifactCore::FloatColor(0.22f, 0.28f, 0.34f, 0.35f);
};

/**
 * @brief グリッドシステムコア
 *
 * MayaのGrid Settingsに相当する設定管理＋描画計算。
 * - 単位（cm/in/pt/px）とDPI連動でピクセル間隔を自動計算
 * - ズーム・パンに応じた有効範囲判定
 * - 主グリッド、細分グリッド、原点軸を個別制御
 */
export class GridSystem {
public:
    explicit GridSystem(const GridSettings& settings = {});
    ~GridSystem() = default;

    // --- 設定 ---
    void setSettings(const GridSettings& s);
    const GridSettings& settings() const;

    void setViewTransform(const GridViewTransform& vt);
    const GridViewTransform& viewTransform() const;

    // --- 計算 ---
    /**
     * @brief 現在のview状態から描画可能なグリッド線を生成
     *
     * @return 各線種ごとの描画リスト（Line, X座標群かY座標群）
     */
    struct GridLines {
        std::vector<float> majorVerticals;   ///< X座標（vertical line X positions）
        std::vector<float> majorHorizontals; ///< Y座標（horizontal line Y positions）
        std::vector<float> minorVerticals;
        std::vector<float> minorHorizontals;
        std::vector<float> axisLines;        ///< 原点線 {x0,y0,x1,y1} 4要素で1線
    };

    GridLines computeVisibleLines() const;

    /** Generate fadeable XZ-plane lines for a 3D ground grid. */
    static std::vector<GroundGridLine> computeGroundGridLines(
        const GroundGridSettings& settings = {});

    /**
     * @brief 指定キャンバス位置にもっとも近いグリッド線の位置を返す
     */
    float snapToGrid(float canvasPos, bool isVertical) const;

    /**
     * @brief 単位座標をビューポートピクセルに変換
     */
    float unitToPixel(float unitValue) const;
    float pixelToUnit(float pixelValue) const;

    /**
     * @brief グリッドをリセット（全設定デフォルト）
     */
    void resetToDefaults();

private:
    GridSettings      settings_;
    GridViewTransform  view_;
    mutable bool       needsRebuild_ = true;

    // キャッシュ
    struct {
        int firstMajorX = 0, lastMajorX = 0;
        int firstMajorY = 0, lastMajorY = 0;
    } visibleRange_;

    void updateCache() const;
    float pixelsPerUnit() const;
};

/** @brief 一つのグリッド定義と計算キャッシュをまとめるレイヤー。 */
export class GridLayer {
public:
    explicit GridLayer(const GridDescriptor& descriptor = {});

    void setDescriptor(const GridDescriptor& descriptor);
    void setVisible(bool visible);
    const GridDescriptor& descriptor() const;
    std::vector<GridLine> computeLines(const GridViewTransform& view, float zoom) const;
    float snap(float canvasPos, bool isVertical) const;
    bool containsPoint(const QPointF& canvasPos) const;

private:
    GridDescriptor descriptor_;
    mutable GridSystem system_;
    mutable float lastAutoStepInterval_ = -1.0f;

    void applyAutoStep(float zoom) const;
    static float niceInterval(float rawInterval);
};

/** @brief 複数の GridLayer を z-order とスナップ優先度付きで管理する。 */
export class GridManager {
public:
    int addLayer(const GridDescriptor& descriptor);
    bool removeLayer(int id);
    void clearLayers();
    bool moveLayer(int id, int targetIndex);
    bool setLayerDescriptor(int id, const GridDescriptor& descriptor);
    const GridDescriptor* layerDescriptor(int id) const;
    std::vector<int> layerIds() const;
    std::vector<GridLine> computeAllLines(const GridViewTransform& view, float zoom) const;
    float snapAll(float canvasPos, bool isVertical) const;
    bool setVisible(int id, bool visible);
    bool isVisible(int id) const;
    int layerCount() const;
    int visibleLayerCount() const;

private:
    struct Entry { int id = -1; GridLayer layer; };
    std::vector<Entry> layers_;
    int nextId_ = 0;
};

/**
 * @brief グリッド描画ヘルパー
 *
 * GridSystemが生成したGridLinesを実際のレンダラー
 * （PrimitiveRenderer2Dなど）で描画するためのユーティリティ。
 */
export class GridRenderer {
public:
    explicit GridRenderer(Artifact::PrimitiveRenderer2D* renderer = nullptr);

    void setRenderer(Artifact::PrimitiveRenderer2D* r);
    Artifact::PrimitiveRenderer2D* renderer() const;

    /**
     * @brief GridLinesを描画
     */
    void draw(const GridSystem& grid, const GridSystem::GridLines& lines);

    /**
     * @brief グリッド数値を描画（将来用）
     */
    void drawLabels(const GridSystem& grid, const GridSystem::GridLines& lines);

private:
    Artifact::PrimitiveRenderer2D* renderer_ = nullptr;
};

// ============================================================
// Inline implementations
// ============================================================

inline GridSystem::GridSystem(const GridSettings& settings)
    : settings_(settings), view_() {
    settings_.majorInterval = std::isfinite(settings_.majorInterval)
        ? std::max(1.0f, settings_.majorInterval) : 100.0f;
    settings_.subdivisions = std::clamp(settings_.subdivisions, 1, 64);
    settings_.visibleRangeStart = std::isfinite(settings_.visibleRangeStart)
        ? settings_.visibleRangeStart : -10000.0f;
    settings_.visibleRangeEnd = std::isfinite(settings_.visibleRangeEnd)
        ? std::max(settings_.visibleRangeStart, settings_.visibleRangeEnd)
        : 10000.0f;
    settings_.cacheValid = false;
}

inline void GridSystem::setSettings(const GridSettings& s) {
    if (&s != &settings_) {
        settings_ = s;
        settings_.majorInterval = std::isfinite(settings_.majorInterval)
            ? std::max(1.0f, settings_.majorInterval) : 100.0f;
        settings_.subdivisions = std::clamp(settings_.subdivisions, 1, 64);
        settings_.visibleRangeStart = std::isfinite(settings_.visibleRangeStart)
            ? settings_.visibleRangeStart : -10000.0f;
        settings_.visibleRangeEnd = std::isfinite(settings_.visibleRangeEnd)
            ? std::max(settings_.visibleRangeStart, settings_.visibleRangeEnd)
            : 10000.0f;
        settings_.cacheValid = false;
        needsRebuild_ = true;
    }
}

inline const GridSettings& GridSystem::settings() const { return settings_; }

inline void GridSystem::setViewTransform(const GridViewTransform& vt) {
    GridViewTransform normalized = vt;
    if (!std::isfinite(normalized.zoom) || normalized.zoom <= 0.0f) {
        normalized.zoom = 1.0f;
    }
    if (normalized.visibleCanvasRect != view_.visibleCanvasRect ||
        normalized.zoom != view_.zoom ||
        normalized.pan != view_.pan ||
        normalized.canvasToViewport != view_.canvasToViewport) {
        view_ = normalized;
        needsRebuild_ = true;
    }
}

inline const GridViewTransform& GridSystem::viewTransform() const { return view_; }

inline float GridSystem::pixelsPerUnit() const {
    if (needsRebuild_ || !settings_.cacheValid) {
        const float dpi = settings_.dpi.factor();
        switch (settings_.unit) {
        case Unit::Pixels:     return 1.0f;
        case Unit::Centimeters: return dpi / 2.54f;
        case Unit::Inches:     return dpi;
        case Unit::Points:     return dpi / 72.0f;
        case Unit::Millimeters:return dpi / 25.4f;
        }
    }
    return settings_.cachedMajorPixelInterval / (settings_.majorInterval > 0.0f ? settings_.majorInterval : 1.0f);
}

inline void GridSystem::updateCache() const {
    if (!needsRebuild_) return;
    const float horizontalPxPerUnit = pixelsPerUnit();
    const float horizontalDpi = settings_.dpi.factor();
    const float verticalPxPerUnit = horizontalPxPerUnit *
        (settings_.dpi.verticalFactor() / horizontalDpi);
    const float pxPerUnit = std::max(horizontalPxPerUnit, verticalPxPerUnit);
    // cached*PixelInterval is a viewport-display metric, so it must include
    // the current zoom.  Canvas line positions remain in canvas units below.
    const float zoom = std::isfinite(view_.zoom) ? std::max(0.0001f, view_.zoom) : 1.0f;
    settings_.cachedMajorPixelInterval = settings_.majorInterval * pxPerUnit * zoom;
    settings_.cachedMinorPixelInterval = settings_.majorInterval * pxPerUnit * zoom /
                                          static_cast<float>(settings_.subdivisions);
    settings_.cacheValid = true;
    needsRebuild_ = false;
}

inline GridSystem::GridLines GridSystem::computeVisibleLines() const {
    updateCache();
    GridLines lines;
    constexpr int kMaxVisibleLinesPerAxis = 8192;

    if (settings_.cachedMajorPixelInterval <= 0.5f) {
        return lines;
    }

    const auto& vt = view_;
    const float viewLeft = std::max(
        static_cast<float>(vt.visibleCanvasRect.left()), settings_.visibleRangeStart);
    const float viewRight = std::min(
        static_cast<float>(vt.visibleCanvasRect.right()), settings_.visibleRangeEnd);
    const float viewTop = std::max(
        static_cast<float>(vt.visibleCanvasRect.top()), settings_.visibleRangeStart);
    const float viewBottom = std::min(
        static_cast<float>(vt.visibleCanvasRect.bottom()), settings_.visibleRangeEnd);
    if (viewRight < viewLeft || viewBottom < viewTop) {
        return lines;
    }

    // --- 主グリッド ---
    if (settings_.showMajor) {
        // visibleCanvasRect and the generated line positions are expressed in
        // canvas units.  The cached pixel interval is only for deciding
        // whether a grid is visually dense enough to draw.
        const float step = settings_.majorInterval;
        if (step > 0.0f) {
            const int firstX = static_cast<int>(std::floor(viewLeft / step));
            const int lastX  = std::min(
                static_cast<int>(std::ceil(viewRight / step)),
                firstX + kMaxVisibleLinesPerAxis - 1);
            const int firstY = static_cast<int>(std::floor(viewTop / step));
            const int lastY  = std::min(
                static_cast<int>(std::ceil(viewBottom / step)),
                firstY + kMaxVisibleLinesPerAxis - 1);

            for (int i = firstX; i <= lastX; ++i) {
                float x = i * step;
                if (x >= viewLeft - 1.0f && x <= viewRight + 1.0f)
                    lines.majorVerticals.push_back(x);
            }
            for (int i = firstY; i <= lastY; ++i) {
                float y = i * step;
                if (y >= viewTop - 1.0f && y <= viewBottom + 1.0f)
                    lines.majorHorizontals.push_back(y);
            }
        }
    }

    // --- 細分グリッド ---
    if (settings_.showMinor && settings_.subdivisions > 0) {
        const float majorStep = settings_.majorInterval;
        const float minorStep = settings_.majorInterval /
                                static_cast<float>(settings_.subdivisions);
        if (minorStep > 0.0f && majorStep > 0.0f) {
            const int firstX = static_cast<int>(std::floor(viewLeft / minorStep));
            const int lastX  = std::min(
                static_cast<int>(std::ceil(viewRight / minorStep)),
                firstX + kMaxVisibleLinesPerAxis - 1);
            const int firstY = static_cast<int>(std::floor(viewTop / minorStep));
            const int lastY  = std::min(
                static_cast<int>(std::ceil(viewBottom / minorStep)),
                firstY + kMaxVisibleLinesPerAxis - 1);

            for (int i = firstX; i <= lastX; ++i) {
                float x = i * minorStep;
                if (i % settings_.subdivisions == 0) continue;
                if (x >= viewLeft - 1.0f && x <= viewRight + 1.0f)
                    lines.minorVerticals.push_back(x);
            }
            for (int i = firstY; i <= lastY; ++i) {
                float y = i * minorStep;
                if (i % settings_.subdivisions == 0) continue;
                if (y >= viewTop - 1.0f && y <= viewBottom + 1.0f)
                    lines.minorHorizontals.push_back(y);
            }
        }
    }

    // --- 原点軸 ---
    if (settings_.showAxis) {
        if (0.0f >= viewLeft - 1.0f && 0.0f <= viewRight + 1.0f)
            lines.axisLines.push_back(0.0f);
        if (0.0f >= viewTop - 1.0f && 0.0f <= viewBottom + 1.0f)
            lines.axisLines.push_back(0.0f);
    }

    return lines;
}

inline float GridSystem::snapToGrid(float canvasPos, bool /*isVertical*/) const {
    updateCache();
    if (!settings_.snapToGrid || !std::isfinite(canvasPos)) return canvasPos;
    const int subdivisions = std::max(1, settings_.subdivisions);
    const float step = settings_.majorInterval / static_cast<float>(subdivisions);
    if (!(step > 0.0f) || !std::isfinite(step)) return canvasPos;
    return std::round(canvasPos / step) * step;
}

inline std::vector<GroundGridLine> GridSystem::computeGroundGridLines(
    const GroundGridSettings& input) {
    GroundGridSettings settings = input;
    settings.majorInterval = std::isfinite(settings.majorInterval)
        ? std::max(0.0001f, settings.majorInterval) : 1.0f;
    settings.subdivisions = std::clamp(settings.subdivisions, 1, 64);
    settings.extent = std::isfinite(settings.extent)
        ? std::max(settings.majorInterval, settings.extent) : 100.0f;
    settings.fadeStart = std::isfinite(settings.fadeStart)
        ? std::clamp(settings.fadeStart, 0.0f, settings.extent) :
          settings.extent * 0.6f;
    settings.fadeEnd = std::isfinite(settings.fadeEnd)
        ? std::clamp(settings.fadeEnd, settings.fadeStart, settings.extent) :
          settings.extent;

    const int majorCount = static_cast<int>(
        std::floor(settings.extent / settings.majorInterval));
    const float minorInterval = settings.majorInterval /
                                 static_cast<float>(settings.subdivisions);
    const int minorCount = static_cast<int>(
        std::floor(settings.extent / minorInterval));
    const int lineCount = std::min(2 * (majorCount + minorCount), 16384);
    NamedVector<GroundGridLine> lines;
    lines.reserve(std::max(0, lineCount));

    const auto fadeColor = [&](const ArtifactCore::FloatColor& source,
                               const float distance) {
        ArtifactCore::FloatColor result = source;
        const float span = settings.fadeEnd - settings.fadeStart;
        const float alpha = span > 0.0f
            ? 1.0f - std::clamp((distance - settings.fadeStart) / span,
                                0.0f, 1.0f)
            : (distance <= settings.fadeEnd ? 1.0f : 0.0f);
        result.setAlpha(result.a() * alpha);
        return result;
    };
    const auto addLine = [&](const float coordinate, const bool major,
                             const bool alongX) {
        const float distance = std::abs(coordinate);
        const auto& baseColor = major ? settings.majorColor : settings.minorColor;
        const float thickness = major ? 1.0f : 0.5f;
        if (alongX) {
            lines.push_back({QVector3D(-settings.extent, 0.0f, coordinate),
                             QVector3D(settings.extent, 0.0f, coordinate),
                             fadeColor(baseColor, distance), thickness, major});
        } else {
            lines.push_back({QVector3D(coordinate, 0.0f, -settings.extent),
                             QVector3D(coordinate, 0.0f, settings.extent),
                             fadeColor(baseColor, distance), thickness, major});
        }
    };
    for (int i = -majorCount; i <= majorCount; ++i) {
        const float coordinate = static_cast<float>(i) * settings.majorInterval;
        addLine(coordinate, true, true);
        addLine(coordinate, true, false);
    }
    for (int i = -minorCount; i <= minorCount; ++i) {
        if (i % settings.subdivisions == 0) continue;
        const float coordinate = static_cast<float>(i) * minorInterval;
        addLine(coordinate, false, true);
        addLine(coordinate, false, false);
    }
    return lines.toStdVector();
}

inline float GridSystem::unitToPixel(float unitValue) const {
    return unitValue * pixelsPerUnit();
}

inline float GridSystem::pixelToUnit(float pixelValue) const {
    return pixelValue / pixelsPerUnit();
}

inline void GridSystem::resetToDefaults() {
    settings_ = GridSettings{};
    settings_.cacheValid = false;
    needsRebuild_ = true;
}

inline GridLayer::GridLayer(const GridDescriptor& descriptor)
    : descriptor_(descriptor), system_(descriptor.baseSettings) {}

inline void GridLayer::setDescriptor(const GridDescriptor& descriptor) {
    descriptor_ = descriptor;
    system_.setSettings(descriptor_.baseSettings);
    lastAutoStepInterval_ = -1.0f;
}

inline void GridLayer::setVisible(bool visible) { descriptor_.visible = visible; }

inline const GridDescriptor& GridLayer::descriptor() const { return descriptor_; }

inline float GridLayer::niceInterval(float rawInterval) {
    if (!std::isfinite(rawInterval) || rawInterval <= 0.0f) return 1.0f;
    const float exponent = std::floor(std::log10(rawInterval));
    const float scale = std::pow(10.0f, exponent);
    const float normalized = rawInterval / scale;
    const float base = normalized <= 1.0f ? 1.0f
        : normalized <= 2.0f ? 2.0f
        : normalized <= 5.0f ? 5.0f : 10.0f;
    return base * scale;
}

inline void GridLayer::applyAutoStep(float zoom) const {
    if (!descriptor_.autoStep.enabled) return;
    const float safeZoom = std::isfinite(zoom) && zoom > 0.0f ? zoom : 1.0f;
    const float target = std::isfinite(descriptor_.autoStep.targetViewportInterval)
        ? std::max(1.0f, descriptor_.autoStep.targetViewportInterval) : 100.0f;
    const float raw = target / safeZoom;
    float interval = descriptor_.autoStep.useNiceNumbers ? niceInterval(raw) : raw;
    if (descriptor_.autoStep.useViewFade) {
        for (const auto& level : descriptor_.autoStep.fadeLevels) {
            if (!std::isfinite(level.zoomMax) || level.zoomMax < 0.0f) continue;
            if (safeZoom <= level.zoomMax) {
                interval *= static_cast<float>(std::max(1, level.majorStepMultiplier));
                break;
            }
        }
    }
    if (std::abs(interval - lastAutoStepInterval_) < 0.0001f) return;

    GridSettings settings = descriptor_.baseSettings;
    settings.majorInterval = interval;
    const int minSub = std::clamp(descriptor_.autoStep.minSubdivisions, 1, 64);
    const int maxSub = std::clamp(std::max(minSub, descriptor_.autoStep.maxSubdivisions), minSub, 64);
    settings.subdivisions = std::clamp(settings.subdivisions, minSub, maxSub);
    if (settings.showMinor && descriptor_.autoStep.useNiceNumbers) {
        // Keep minor lines in a readable viewport range as zoom changes.
        // Prefer the common 1/2/5 subdivision sequence, while respecting
        // explicit bounds from the descriptor.
        const float minorTarget = target * 0.25f;
        const float majorPixels = interval * safeZoom;
        int selected = minSub;
        for (const int candidate : {1, 2, 4, 5, 10, 20, 40, 64}) {
            if (candidate < minSub || candidate > maxSub) continue;
            const float minorPixels = majorPixels / static_cast<float>(candidate);
            if (minorPixels >= minorTarget * 0.5f &&
                minorPixels <= target * 0.8f) {
                selected = candidate;
                break;
            }
            if (minorPixels > minorTarget) selected = candidate;
        }
        settings.subdivisions = std::clamp(selected, minSub, maxSub);
    }
    system_.setSettings(settings);
    lastAutoStepInterval_ = interval;
}

inline std::vector<GridLine> GridLayer::computeLines(
    const GridViewTransform& view, float zoom) const {
    if (!descriptor_.visible) return {};
    applyAutoStep(zoom);
    system_.setViewTransform(view);
    const auto generated = system_.computeVisibleLines();
    const auto& settings = system_.settings();
    const QRectF rect = view.visibleCanvasRect;
    const auto lineInRegion = [this](const QPointF& start, const QPointF& end) {
        if (descriptor_.regions.empty()) return true;
        const QPointF midpoint = (start + end) * 0.5;
        return containsPoint(midpoint);
    };
    const auto applyGlobalAlpha = [this](std::vector<GridLine>& lines) {
        const float alpha = std::isfinite(descriptor_.globalAlpha)
            ? std::clamp(descriptor_.globalAlpha, 0.0f, 1.0f) : 1.0f;
        for (auto& line : lines) {
            line.color.setAlpha(std::clamp(line.color.alpha() * alpha, 0.0f, 1.0f));
        }
    };
    const auto applyViewFade = [this, zoom](std::vector<GridLine>& lines) {
        if (!descriptor_.autoStep.useViewFade ||
            descriptor_.autoStep.fadeLevels.empty()) return;
        float fadeAlpha = 1.0f;
        int multiplier = 1;
        bool selected = false;
        for (const auto& level : descriptor_.autoStep.fadeLevels) {
            if (!std::isfinite(level.zoomMax) || !std::isfinite(level.alpha) ||
                level.zoomMax < 0.0f) continue;
            if (zoom <= level.zoomMax) {
                fadeAlpha = std::clamp(level.alpha, 0.0f, 1.0f);
                multiplier = std::max(1, level.majorStepMultiplier);
                selected = true;
                break;
            }
        }
        if (!selected) return;
        for (auto& line : lines) {
            if (!line.isMajor && multiplier > 1) {
                line.color.setAlpha(0.0f);
            } else {
                line.color.setAlpha(std::clamp(line.color.alpha() * fadeAlpha,
                                               0.0f, 1.0f));
            }
        }
    };
    std::vector<GridLine> result;

    if (descriptor_.type == GridType::Custom && descriptor_.customGenerator) {
        try {
            result = descriptor_.customGenerator(descriptor_, view);
        } catch (...) {
            result.clear();
        }
        for (auto& line : result) {
            if (!std::isfinite(line.thickness) || line.thickness < 0.0f)
                line.thickness = 1.0f;
            line.color.setAlpha(std::clamp(
                line.color.alpha() *
                    (std::isfinite(descriptor_.globalAlpha)
                         ? std::clamp(descriptor_.globalAlpha, 0.0f, 1.0f)
                         : 1.0f),
                0.0f, 1.0f));
        }
        return result;
    }

    if (descriptor_.type == GridType::Polar) {
        const QPointF center = descriptor_.origin;
        const float radius = static_cast<float>(std::hypot(
            std::max(std::abs(rect.left() - center.x()), std::abs(rect.right() - center.x())),
            std::max(std::abs(rect.top() - center.y()), std::abs(rect.bottom() - center.y()))));
        const float radialStep = std::isfinite(descriptor_.radialInterval) &&
                                 descriptor_.radialInterval > 0.0f
            ? descriptor_.radialInterval : settings.majorInterval;
        const int angleCount = std::clamp(descriptor_.angularSubdivisions, 3, 360);
        const float pi = 3.14159265358979323846f;
        for (float r = radialStep; r <= radius; r += radialStep) {
            constexpr int kSegments = 96;
            for (int i = 0; i < kSegments; ++i) {
                const float a0 = 2.0f * pi * static_cast<float>(i) / kSegments;
                const float a1 = 2.0f * pi * static_cast<float>(i + 1) / kSegments;
                const QPointF start{center.x() + r * std::cos(a0), center.y() + r * std::sin(a0)};
                const QPointF end{center.x() + r * std::cos(a1), center.y() + r * std::sin(a1)};
                if (lineInRegion(start, end)) {
                    result.push_back({start, end, settings.majorColor,
                                      settings.majorStyle.thickness,
                                      settings.majorStyle.style, true});
                    if (descriptor_.showLabels && i == 0) {
                        result.back().label = QStringLiteral("r %1").arg(
                            QString::number(r, 'f', r < 1.0f ? 2 : 0));
                    }
                }
            }
        }
        for (int i = 0; i < angleCount; ++i) {
            const float angle = 2.0f * pi * static_cast<float>(i) / angleCount;
            const QPointF end{center.x() + radius * std::cos(angle),
                              center.y() + radius * std::sin(angle)};
            if (lineInRegion(center, end)) {
                result.push_back({center, end, settings.minorColor,
                                  settings.minorStyle.thickness,
                                  settings.minorStyle.style, false});
                if (descriptor_.showLabels) {
                    result.back().label = QStringLiteral("%1°").arg(
                        QString::number(static_cast<float>(i) * 360.0f /
                                        static_cast<float>(angleCount), 'f', 0));
                }
            }
        }
        applyGlobalAlpha(result);
        applyViewFade(result);
        return result;
    }

    if (descriptor_.type == GridType::Radial) {
        const QPointF center = descriptor_.origin;
        const float radius = static_cast<float>(std::hypot(
            std::max(std::abs(rect.left() - center.x()), std::abs(rect.right() - center.x())),
            std::max(std::abs(rect.top() - center.y()), std::abs(rect.bottom() - center.y()))));
        const int angleCount = std::clamp(descriptor_.angularSubdivisions, 3, 360);
        const float pi = 3.14159265358979323846f;
        for (int i = 0; i < angleCount; ++i) {
            const float angle = 2.0f * pi * static_cast<float>(i) / angleCount;
            const QPointF end{center.x() + radius * std::cos(angle),
                              center.y() + radius * std::sin(angle)};
            if (lineInRegion(center, end)) {
                result.push_back({center, end, settings.majorColor,
                                  settings.majorStyle.thickness,
                                  settings.majorStyle.style, true});
                if (descriptor_.showLabels) {
                    result.back().label = QStringLiteral("%1°").arg(
                        QString::number(static_cast<float>(i) * 360.0f /
                                        static_cast<float>(angleCount), 'f', 0));
                }
            }
        }
        applyGlobalAlpha(result);
        applyViewFade(result);
        return result;
    }

    if (descriptor_.type == GridType::Isometric) {
        const float spacing = std::isfinite(settings.majorInterval) && settings.majorInterval > 0.0f
            ? settings.majorInterval : 100.0f;
        const float height = static_cast<float>(rect.height());
        const float width = static_cast<float>(rect.width());
        const float halfSpan = width + height * 0.57735026919f;
        const int count = std::min(8192, std::max(1,
            static_cast<int>(std::ceil((width + height) / spacing)) + 2));
        for (int i = -count; i <= count; ++i) {
            const float offset = static_cast<float>(i) * spacing;
            const QPointF a{rect.left() - halfSpan + offset, rect.top()};
            const QPointF b{rect.left() + halfSpan + offset, rect.bottom()};
            const QPointF c{rect.left() - halfSpan + offset, rect.bottom()};
            const QPointF d{rect.left() + halfSpan + offset, rect.top()};
            if (lineInRegion(a, b))
                result.push_back({a, b, settings.majorColor, settings.majorStyle.thickness,
                                  settings.majorStyle.style, true});
            if (lineInRegion(c, d))
                result.push_back({c, d, settings.majorColor, settings.majorStyle.thickness,
                                  settings.majorStyle.style, true});
        }
        applyGlobalAlpha(result);
        applyViewFade(result);
        return result;
    }

    if (descriptor_.type == GridType::Perspective) {
        const QPointF vanishing = descriptor_.vanishingPoint;
        const float bottom = static_cast<float>(rect.bottom());
        const int lineCount = std::clamp(descriptor_.perspectiveLineCount, 2, 256);
        for (int i = 0; i <= lineCount; ++i) {
            const float t = static_cast<float>(i) / lineCount;
            const QPointF edge{rect.left() + t * rect.width(), bottom};
            if (lineInRegion(vanishing, edge))
                result.push_back({vanishing, edge, settings.majorColor,
                                  settings.majorStyle.thickness, settings.majorStyle.style, true});
        }
        const float horizon = std::isfinite(descriptor_.horizonY)
            ? descriptor_.horizonY : static_cast<float>(rect.center().y());
        const float spacing = std::max(1.0f, std::abs(settings.majorInterval));
        for (float y = horizon; y <= bottom; y += spacing) {
            const QPointF start{rect.left(), y};
            const QPointF end{rect.right(), y};
            if (lineInRegion(start, end))
                result.push_back({start, end, settings.minorColor, settings.minorStyle.thickness,
                                  settings.minorStyle.style, false});
        }
        if (descriptor_.showLabels) {
            for (auto& line : result) {
                if (std::abs(line.start.y() - line.end.y()) < 0.0001 &&
                    std::isfinite(line.start.y())) {
                    line.label = QStringLiteral("y %1").arg(
                        QString::number(line.start.y(), 'f',
                                        std::abs(line.start.y()) < 1.0 ? 2 : 0));
                }
            }
        }
        applyGlobalAlpha(result);
        applyViewFade(result);
        return result;
    }

    if (descriptor_.type != GridType::Rectangular) return result;
    result.reserve(generated.majorVerticals.size() + generated.majorHorizontals.size() +
                   generated.minorVerticals.size() + generated.minorHorizontals.size());
    for (const float x : generated.majorVerticals) {
        const QPointF start{x, rect.top()};
        const QPointF end{x, rect.bottom()};
        if (lineInRegion(start, end))
            result.push_back({start, end, settings.majorColor, settings.majorStyle.thickness,
                              settings.majorStyle.style, true});
    }
    for (const float y : generated.majorHorizontals) {
        const QPointF start{rect.left(), y};
        const QPointF end{rect.right(), y};
        if (lineInRegion(start, end))
            result.push_back({start, end, settings.majorColor, settings.majorStyle.thickness,
                              settings.majorStyle.style, true});
    }
    for (const float x : generated.minorVerticals) {
        const QPointF start{x, rect.top()};
        const QPointF end{x, rect.bottom()};
        if (lineInRegion(start, end))
            result.push_back({start, end, settings.minorColor, settings.minorStyle.thickness,
                              settings.minorStyle.style, false});
    }
    for (const float y : generated.minorHorizontals) {
        const QPointF start{rect.left(), y};
        const QPointF end{rect.right(), y};
        if (lineInRegion(start, end))
            result.push_back({start, end, settings.minorColor, settings.minorStyle.thickness,
                              settings.minorStyle.style, false});
    }
    for (auto& line : result) {
        const QPointF midpoint = (line.start + line.end) * 0.5;
        const GridSettings* overrideSettings = nullptr;
        for (const auto& region : descriptor_.regions) {
            if (region.enabled && region.overrideSettings &&
                region.canvasRect.contains(midpoint)) {
                overrideSettings = &*region.overrideSettings;
                break;
            }
        }
        if (overrideSettings) {
            const auto& style = line.isMajor ? overrideSettings->majorStyle
                                             : overrideSettings->minorStyle;
            line.color = line.isMajor ? overrideSettings->majorColor
                                      : overrideSettings->minorColor;
            line.thickness = style.thickness;
            line.style = style.style;
        }
    }
    if (descriptor_.showLabels) {
        for (auto& line : result) {
            if (!line.isMajor) continue;
            const bool vertical = std::abs(line.start.x() - line.end.x()) < 0.0001;
            const double value = vertical ? line.start.x() : line.start.y();
            if (std::isfinite(value)) {
                line.label = QString::number(value, 'f',
                                              std::abs(value) < 1.0 ? 2 : 0);
            }
        }
    }
    applyGlobalAlpha(result);
    applyViewFade(result);
    return result;
}

inline float GridLayer::snap(float canvasPos, bool isVertical) const {
    if (!descriptor_.visible || !descriptor_.snapEnabled) return canvasPos;
    return system_.snapToGrid(canvasPos, isVertical);
}

inline bool GridLayer::containsPoint(const QPointF& canvasPos) const {
    if (descriptor_.regions.empty()) return true;
    return std::any_of(descriptor_.regions.begin(), descriptor_.regions.end(),
                       [&canvasPos](const GridRegion& region) {
                           return region.enabled && region.canvasRect.contains(canvasPos);
                       });
}

inline int GridManager::addLayer(const GridDescriptor& descriptor) {
    const int id = nextId_++;
    layers_.push_back({id, GridLayer(descriptor)});
    for (int index = 0; index < static_cast<int>(layers_.size()); ++index) {
        auto normalized = layers_[static_cast<std::size_t>(index)].layer.descriptor();
        normalized.zOrder = index;
        layers_[static_cast<std::size_t>(index)].layer.setDescriptor(normalized);
    }
    return id;
}

inline bool GridManager::removeLayer(int id) {
    const auto it = std::find_if(layers_.begin(), layers_.end(),
                                 [id](const Entry& entry) { return entry.id == id; });
    if (it == layers_.end()) return false;
    layers_.erase(it);
    for (int index = 0; index < static_cast<int>(layers_.size()); ++index) {
        auto descriptor = layers_[static_cast<std::size_t>(index)].layer.descriptor();
        descriptor.zOrder = index;
        layers_[static_cast<std::size_t>(index)].layer.setDescriptor(descriptor);
    }
    return true;
}

inline void GridManager::clearLayers() {
    layers_.clear();
}

inline bool GridManager::moveLayer(int id, int targetIndex) {
    if (targetIndex < 0 || targetIndex >= static_cast<int>(layers_.size()))
        return false;
    const auto it = std::find_if(layers_.begin(), layers_.end(),
                                 [id](const Entry& entry) { return entry.id == id; });
    if (it == layers_.end()) return false;
    const int currentIndex = static_cast<int>(std::distance(layers_.begin(), it));
    if (currentIndex == targetIndex) return true;
    Entry moved = std::move(*it);
    layers_.erase(layers_.begin() + currentIndex);
    layers_.insert(layers_.begin() + targetIndex, std::move(moved));
    for (int index = 0; index < static_cast<int>(layers_.size()); ++index) {
        auto descriptor = layers_[static_cast<std::size_t>(index)].layer.descriptor();
        descriptor.zOrder = index;
        layers_[static_cast<std::size_t>(index)].layer.setDescriptor(descriptor);
    }
    return true;
}

inline bool GridManager::setLayerDescriptor(int id, const GridDescriptor& descriptor) {
    for (auto& entry : layers_) {
        if (entry.id == id) {
            entry.layer.setDescriptor(descriptor);
            for (int index = 0; index < static_cast<int>(layers_.size()); ++index) {
                auto normalized = layers_[static_cast<std::size_t>(index)].layer.descriptor();
                normalized.zOrder = index;
                layers_[static_cast<std::size_t>(index)].layer.setDescriptor(normalized);
            }
            return true;
        }
    }
    return false;
}

inline const GridDescriptor* GridManager::layerDescriptor(int id) const {
    for (const auto& entry : layers_)
        if (entry.id == id) return &entry.layer.descriptor();
    return nullptr;
}

inline std::vector<int> GridManager::layerIds() const {
    NamedVector<int> ids;
    ids.reserve(layers_.size());
    for (const auto& entry : layers_)
        ids.push_back(entry.id);
    return ids.toStdVector();
}

inline std::vector<GridLine> GridManager::computeAllLines(
    const GridViewTransform& view, float zoom) const {
    NamedVector<const Entry*> visible;
    for (const auto& entry : layers_)
        if (entry.layer.descriptor().visible) visible.push_back(&entry);
    std::sort(visible.begin(), visible.end(), [](const Entry* a, const Entry* b) {
        return a->layer.descriptor().zOrder < b->layer.descriptor().zOrder;
    });
    NamedVector<GridLine> result;
    for (const Entry* entry : visible) {
        auto lines = entry->layer.computeLines(view, zoom);
        result.insert(result.end(), lines.begin(), lines.end());
    }
    return result.toStdVector();
}

inline float GridManager::snapAll(float canvasPos, bool isVertical) const {
    const GridLayer* best = nullptr;
    float bestCandidate = canvasPos;
    float bestDistance = std::numeric_limits<float>::infinity();
    float bestPriority = -std::numeric_limits<float>::infinity();
    for (const auto& entry : layers_) {
        const auto& descriptor = entry.layer.descriptor();
        if (!descriptor.visible || !descriptor.snapEnabled) continue;
        const float candidate = entry.layer.snap(canvasPos, isVertical);
        if (!std::isfinite(candidate)) continue;
        const float distance = std::abs(candidate - canvasPos);
        const bool closer = distance < bestDistance - 0.0001f;
        const bool equalAndHigherPriority =
            std::abs(distance - bestDistance) <= 0.0001f &&
            descriptor.snapPriority > bestPriority;
        if (closer || equalAndHigherPriority) {
            best = &entry.layer;
            bestCandidate = candidate;
            bestDistance = distance;
            bestPriority = descriptor.snapPriority;
        }
    }
    return best ? bestCandidate : canvasPos;
}

inline bool GridManager::setVisible(int id, bool visible) {
    for (auto& entry : layers_) {
        if (entry.id == id) { entry.layer.setVisible(visible); return true; }
    }
    return false;
}

inline bool GridManager::isVisible(int id) const {
    const auto* descriptor = layerDescriptor(id);
    return descriptor && descriptor->visible;
}

inline int GridManager::visibleLayerCount() const {
    return static_cast<int>(std::count_if(layers_.begin(), layers_.end(),
        [](const Entry& entry) { return entry.layer.descriptor().visible; }));
}

inline int GridManager::layerCount() const {
    return static_cast<int>(layers_.size());
}

// GridRenderer implementation is provided in the corresponding implementation
// unit (ArtifactCore/src/Grid/ArtifactGridSystem.cppm) because it depends on
// Artifact::PrimitiveRenderer2D which is defined in the Artifact module.

} // namespace Artifact::Grid
