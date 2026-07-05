module;
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <variant>
#include <cmath>
#include <algorithm>
#include <QRectF>
#include <QTransform>

export module Artifact.Grid.System;

import Particle;
import Color.Float;
// PrimitiveRenderer2D is declared in Artifact; forward-declare to avoid
// requiring the Artifact.Render.PrimitiveRenderer2D module at this header
// level. The full module is used in implementation files where needed.
namespace Artifact { class PrimitiveRenderer2D; }

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
    float factor() const { return horizontalDpi; }
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
    : settings_(settings), view_() {}

inline void GridSystem::setSettings(const GridSettings& s) {
    if (&s != &settings_) {
        settings_ = s;
        needsRebuild_ = true;
    }
}

inline const GridSettings& GridSystem::settings() const { return settings_; }

inline void GridSystem::setViewTransform(const GridViewTransform& vt) {
    if (vt.visibleCanvasRect != view_.visibleCanvasRect ||
        vt.zoom != view_.zoom ||
        vt.pan != view_.pan ||
        !vt.canvasToViewport.isIdentity()) {
        view_ = vt;
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
    const float pxPerUnit = pixelsPerUnit();
    settings_.cachedMajorPixelInterval = settings_.majorInterval * pxPerUnit;
    settings_.cachedMinorPixelInterval = settings_.majorInterval * pxPerUnit /
                                          static_cast<float>(settings_.subdivisions);
    settings_.cacheValid = true;
    needsRebuild_ = false;
}

inline GridSystem::GridLines GridSystem::computeVisibleLines() const {
    updateCache();
    GridLines lines;

    if (settings_.cachedMajorPixelInterval <= 0.5f) {
        return lines;
    }

    const auto& vt = view_;
    const float viewLeft   = vt.visibleCanvasRect.left();
    const float viewRight  = vt.visibleCanvasRect.right();
    const float viewTop    = vt.visibleCanvasRect.top();
    const float viewBottom = vt.visibleCanvasRect.bottom();

    // --- 主グリッド ---
    if (settings_.showMajor) {
        const float step = settings_.cachedMajorPixelInterval;
        if (step > 0.0f) {
            const int firstX = static_cast<int>(std::floor(viewLeft / step));
            const int lastX  = static_cast<int>(std::ceil(viewRight / step));
            const int firstY = static_cast<int>(std::floor(viewTop / step));
            const int lastY  = static_cast<int>(std::ceil(viewBottom / step));

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
        const float majorStep = settings_.cachedMajorPixelInterval;
        const float minorStep = settings_.cachedMinorPixelInterval;
        if (minorStep > 0.0f && majorStep > 0.0f) {
            const int firstX = static_cast<int>(std::floor(viewLeft / minorStep));
            const int lastX  = static_cast<int>(std::ceil(viewRight / minorStep));
            const int firstY = static_cast<int>(std::floor(viewTop / minorStep));
            const int lastY  = static_cast<int>(std::ceil(viewBottom / minorStep));

            for (int i = firstX; i <= lastX; ++i) {
                float x = i * minorStep;
                if (std::fmod(std::fabs(x), majorStep) < 0.5f) continue;
                if (x >= viewLeft - 1.0f && x <= viewRight + 1.0f)
                    lines.minorVerticals.push_back(x);
            }
            for (int i = firstY; i <= lastY; ++i) {
                float y = i * minorStep;
                if (std::fmod(std::fabs(y), majorStep) < 0.5f) continue;
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
    const float step = settings_.cachedMajorPixelInterval;
    if (step <= 0.0f) return canvasPos;
    return std::round(canvasPos / step) * step;
}

inline float GridSystem::unitToPixel(float unitValue) const {
    return unitValue * pixelsPerUnit();
}

inline float GridSystem::pixelToUnit(float pixelValue) const {
    return pixelValue / pixelsPerUnit();
}

inline void GridSystem::resetToDefaults() {
    settings_ = GridSettings{};
    needsRebuild_ = true;
}

// GridRenderer implementation is provided in the corresponding implementation
// unit (ArtifactCore/src/Grid/ArtifactGridSystem.cppm) because it depends on
// Artifact::PrimitiveRenderer2D which is defined in the Artifact module.

} // namespace Artifact::Grid
