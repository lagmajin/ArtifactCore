module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include "../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Core.Transform.Viewport;

import Core.Scale.Zoom;

export namespace ArtifactCore {
    using namespace Diligent;

    /**
     * @brief マウス操作（パン・ズーム）とシェーダー用座標変換を一括管理するクラス
     * Coreライブラリにロジックを集約することで、UI側（Qt）と描画側（Diligent）の計算を一致させます。
     */
    class LIBRARY_DLL_API ViewportTransformer {
    public:
        ViewportTransformer();
        ~ViewportTransformer();

        ViewportTransformer(const ViewportTransformer&) = delete;
        ViewportTransformer& operator=(const ViewportTransformer&) = delete;

        ViewportTransformer(ViewportTransformer&& other) noexcept;
        ViewportTransformer& operator=(ViewportTransformer&& other) noexcept;

        // 状態設定
        void SetViewportSize(float w, float h);
        void SetCanvasSize(float w, float h);
        void SetPan(float x, float y);
        void SetZoom(float zoom);
        void PanBy(float dx, float dy);
 
        // 高度な操作
        void ZoomAroundViewportPoint(float2 viewportPos, float newZoom);
        void FitCanvasToViewport(float margin = 50.0f);
        void FillCanvasToViewport(float margin = 0.0f);
        void ResetView();

        // 状態取得
        float2 GetViewportSize() const;
        float2 GetCanvasSize() const;
        float2 GetPan() const;
        float GetZoom() const;

        // 座標変換ロジック
        float2 CanvasToViewport(float2 canvasPos) const;
        float2 ViewportToCanvas(float2 viewportPos) const;
        
        /**
         * @brief キャンバス座標をNDC（-1.0〜1.0）へ変換します
         * シェーダーでの頂点配置に利用します。
         */
        float2 CanvasToNDC(float2 canvasPos) const;

        /**
         * @brief 定数バッファ用の構造体を取得します
         */
        struct ViewportCB {
            float2 offset;      // パン位置
            float2 scale;       // スケール（通常 1.0）
            float2 screenSize;  // ビューポート解像度
            float  zoom;        // ズーム倍率
            float  padding;
        };
        ViewportCB GetViewportCB() const;

    private:
        class Impl;
        Impl* impl_;
    };

}
