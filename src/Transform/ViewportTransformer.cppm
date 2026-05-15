module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <algorithm>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QDebug>
module Core.Transform.Viewport;

import Core.Scale.Zoom;

namespace ArtifactCore {
    using namespace Diligent;

    class ViewportTransformer::Impl {
    public:
        float2 viewportSize = {1920, 1080};
        float2 canvasSize = {1920, 1080};
        float2 pan = {0, 0};
        float zoom = 1.0f;

        float2 CanvasToViewport(float2 canvasPos) const {
            // (Pos * Zoom) + Pan
            return { (canvasPos.x * zoom) + pan.x, (canvasPos.y * zoom) + pan.y };
        }

        float2 ViewportToCanvas(float2 viewportPos) const {
            // (Pos - Pan) / Zoom
            return { (viewportPos.x - pan.x) / zoom, (viewportPos.y - pan.y) / zoom };
        }

        float2 CanvasToNDC(float2 canvasPos) const {
            float2 v = CanvasToViewport(canvasPos);
            // v is now in screen pixels [0...viewportWidth, 0...viewportHeight]
            // NDC in Diligent is [-1...1, 1...-1] (top-left is -1, 1)
            float2 ndc = { 
                (v.x / viewportSize.x) * 2.0f - 1.0f,
                -((v.y / viewportSize.y) * 2.0f - 1.0f) 
            };
            return ndc;
        }
    };

    ViewportTransformer::ViewportTransformer() : impl_(new Impl()) {}

    ViewportTransformer::ViewportTransformer(ViewportTransformer&& other) noexcept : impl_(other.impl_) {
        other.impl_ = nullptr;
    }

    ViewportTransformer& ViewportTransformer::operator=(ViewportTransformer&& other) noexcept {
        if (this != &other) {
            delete impl_;
            impl_ = other.impl_;
            other.impl_ = nullptr;
        }
        return *this;
    }

    ViewportTransformer::~ViewportTransformer() { delete impl_; }

    void ViewportTransformer::SetViewportSize(float w, float h) { impl_->viewportSize = {w, h}; }
    void ViewportTransformer::SetCanvasSize(float w, float h) { impl_->canvasSize = {w, h}; }
    void ViewportTransformer::SetPan(float x, float y) { impl_->pan = {x, y}; }
    void ViewportTransformer::SetZoom(float zoom) { impl_->zoom = std::max(0.001f, zoom); }
 
    void ViewportTransformer::PanBy(float dx, float dy) {
        impl_->pan.x += dx;
        impl_->pan.y += dy;
    }

    void ViewportTransformer::ResetView() {
        impl_->pan = {0, 0};
        impl_->zoom = 1.0f;
    }

    void ViewportTransformer::ZoomAroundViewportPoint(float2 viewportPos, float newZoom) {
        float2 canvasPos = ViewportToCanvas(viewportPos);
        impl_->zoom = std::max(0.001f, newZoom);
        
        // Adjust pan so that canvasPos remains under viewportPos
        // viewportPos = (canvasPos * newZoom) + newPan
        // newPan = viewportPos - (canvasPos * newZoom)
        impl_->pan.x = viewportPos.x - (canvasPos.x * impl_->zoom);
        impl_->pan.y = viewportPos.y - (canvasPos.y * impl_->zoom);
    }

    void ViewportTransformer::FitCanvasToViewport(float margin) {
        if (impl_->canvasSize.x <= 0 || impl_->canvasSize.y <= 0) return;

        float availableW = impl_->viewportSize.x - margin * 2.0f;
        float availableH = impl_->viewportSize.y - margin * 2.0f;

        float zoomW = availableW / impl_->canvasSize.x;
        float zoomH = availableH / impl_->canvasSize.y;

        impl_->zoom = std::min(zoomW, zoomH);
        
        // Center the canvas
        impl_->pan.x = (impl_->viewportSize.x - (impl_->canvasSize.x * impl_->zoom)) * 0.5f;
        impl_->pan.y = (impl_->viewportSize.y - (impl_->canvasSize.y * impl_->zoom)) * 0.5f;

        qDebug() << "[ViewportTransformer] FitCanvasToViewport:"
                 << "viewport=" << impl_->viewportSize.x << "x" << impl_->viewportSize.y
                 << "canvas=" << impl_->canvasSize.x << "x" << impl_->canvasSize.y
                 << "zoom=" << impl_->zoom
                 << "pan=(" << impl_->pan.x << "," << impl_->pan.y << ")";
    }

    void ViewportTransformer::FillCanvasToViewport(float margin) {
        if (impl_->canvasSize.x <= 0 || impl_->canvasSize.y <= 0) return;

        float availableW = impl_->viewportSize.x - margin * 2.0f;
        float availableH = impl_->viewportSize.y - margin * 2.0f;
        if (availableW <= 0.0f || availableH <= 0.0f) {
            return;
        }

        float zoomW = availableW / impl_->canvasSize.x;
        float zoomH = availableH / impl_->canvasSize.y;

        impl_->zoom = std::max(zoomW, zoomH);

        // Center the canvas, then allow the canvas to crop beyond viewport edges.
        impl_->pan.x = (impl_->viewportSize.x - (impl_->canvasSize.x * impl_->zoom)) * 0.5f;
        impl_->pan.y = (impl_->viewportSize.y - (impl_->canvasSize.y * impl_->zoom)) * 0.5f;

        qDebug() << "[ViewportTransformer] FillCanvasToViewport:"
                 << "viewport=" << impl_->viewportSize.x << "x" << impl_->viewportSize.y
                 << "canvas=" << impl_->canvasSize.x << "x" << impl_->canvasSize.y
                 << "zoom=" << impl_->zoom
                 << "pan=(" << impl_->pan.x << "," << impl_->pan.y << ")";
    }

    float2 ViewportTransformer::GetViewportSize() const { return impl_->viewportSize; }
    float2 ViewportTransformer::GetCanvasSize() const { return impl_->canvasSize; }
    float2 ViewportTransformer::GetPan() const { return impl_->pan; }
    float ViewportTransformer::GetZoom() const { return impl_->zoom; }

    float2 ViewportTransformer::CanvasToViewport(float2 canvasPos) const { return impl_->CanvasToViewport(canvasPos); }
    float2 ViewportTransformer::ViewportToCanvas(float2 viewportPos) const { return impl_->ViewportToCanvas(viewportPos); }
    float2 ViewportTransformer::CanvasToNDC(float2 canvasPos) const { return impl_->CanvasToNDC(canvasPos); }

    ViewportTransformer::ViewportCB ViewportTransformer::GetViewportCB() const {
        return { impl_->pan, {1.0f, 1.0f}, impl_->viewportSize, impl_->zoom, 0.0f };
    }
}
