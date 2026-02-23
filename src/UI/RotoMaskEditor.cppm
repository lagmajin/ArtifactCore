module;

#include <QWidget>
#include <QPointF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <QColor>
#include <vector>
#include <cmath>

module Core.UI.RotoMaskEditor;

import std;
import Core.Mask.RotoMask;

namespace ArtifactCore {

// ============================================================================
// RotoMaskEditor::Impl
// ============================================================================

class RotoMaskEditor::Impl {
public:
    RotoMask* mask = nullptr;
    double currentTime = 0.0;
    
    RotoEditMode editMode = RotoEditMode::Draw;
    RotoMask::Interpolation defaultInterpolation = RotoMask::Interpolation::Bezier;
    
    // 表示設定
    bool showGrid = true;
    int gridSize = 20;
    double zoom = 1.0;
    QPointF pan{0, 0};
    
    // 選択状態
    RotoSelection selection;
    
    // ドラッグ状態
    bool isDragging = false;
    QPointF dragStartPos;
    QPointF dragOriginalValue;
    
    // 描画中の新しい頂点位置
    bool isDrawing = false;
    QPointF lastDrawPos;
    
    // アンドゥ用履歴（簡易実装）
    std::vector<RotoMask> undoStack;
    std::vector<RotoMask> redoStack;
    int maxUndoCount = 50;
    
    // 表示色
    QColor pathColor{255, 255, 255};
    QColor vertexColor{0, 200, 255};
    QColor selectedVertexColor{255, 200, 0};
    QColor tangentColor{100, 255, 100};
    QColor tangentLineColor{100, 100, 100};
    QColor gridColor{50, 50, 50};
    
    void saveUndoState() {
        if (mask) {
            undoStack.push_back(mask->clone());
            if (static_cast<int>(undoStack.size()) > maxUndoCount) {
                undoStack.erase(undoStack.begin());
            }
            redoStack.clear();
        }
    }
    
    std::vector<RotoVertex> getCurrentVertices() const {
        if (!mask) return {};
        return mask->sampleVertices(currentTime);
    }
};

// ============================================================================
// RotoMaskEditor 実装
// ============================================================================

RotoMaskEditor::RotoMaskEditor(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
}

RotoMaskEditor::~RotoMaskEditor() {
    delete impl_;
}

// ========================================
// マスク設定
// ========================================

void RotoMaskEditor::setMask(RotoMask* mask) {
    impl_->mask = mask;
    impl_->undoStack.clear();
    impl_->redoStack.clear();
    update();
}

RotoMask* RotoMaskEditor::mask() const {
    return impl_->mask;
}

void RotoMaskEditor::setCurrentTime(double time) {
    impl_->currentTime = time;
    update();
}

double RotoMaskEditor::currentTime() const {
    return impl_->currentTime;
}

// ========================================
// 編集モード
// ========================================

void RotoMaskEditor::setEditMode(RotoEditMode mode) {
    impl_->editMode = mode;
    
    // モードに応じたカーサー
    switch (mode) {
        case RotoEditMode::Select:
            setCursor(Qt::ArrowCursor);
            break;
        case RotoEditMode::Draw:
            setCursor(Qt::CrossCursor);
            break;
        case RotoEditMode::Edit:
            setCursor(Qt::SizeAllCursor);
            break;
        case RotoEditMode::Delete:
            setCursor(Qt::ForbiddenCursor);
            break;
    }
    
    Q_EMIT editModeChanged(mode);
    update();
}

RotoEditMode RotoMaskEditor::editMode() const {
    return impl_->editMode;
}

void RotoMaskEditor::setDefaultInterpolation(RotoMask::Interpolation interp) {
    impl_->defaultInterpolation = interp;
}

// ========================================
// 表示設定
// ========================================

void RotoMaskEditor::setShowGrid(bool show) {
    impl_->showGrid = show;
    update();
}

bool RotoMaskEditor::showGrid() const {
    return impl_->showGrid;
}

void RotoMaskEditor::setGridSize(int size) {
    impl_->gridSize = size;
    update();
}

int RotoMaskEditor::gridSize() const {
    return impl_->gridSize;
}

void RotoMaskEditor::setZoom(double zoom) {
    impl_->zoom = std::clamp(zoom, 0.1, 20.0);
    Q_EMIT zoomChanged(impl_->zoom);
    update();
}

double RotoMaskEditor::zoom() const {
    return impl_->zoom;
}

void RotoMaskEditor::setPan(const QPointF& pan) {
    impl_->pan = pan;
    update();
}

QPointF RotoMaskEditor::pan() const {
    return impl_->pan;
}

// ========================================
// 選択
// ========================================

RotoSelection RotoMaskEditor::selection() const {
    return impl_->selection;
}

void RotoMaskEditor::clearSelection() {
    impl_->selection = RotoSelection();
    Q_EMIT selectionChanged(impl_->selection);
    update();
}

void RotoMaskEditor::selectVertex(int vertexIndex) {
    impl_->selection.type = SelectionType::Vertex;
    impl_->selection.vertexIndex = vertexIndex;
    Q_EMIT selectionChanged(impl_->selection);
    update();
}

// ========================================
// 座標変換
// ========================================

QPointF RotoMaskEditor::screenToMask(const QPointF& screenPos) const {
    return (screenPos - impl_->pan) / impl_->zoom;
}

QPointF RotoMaskEditor::maskToScreen(const QPointF& maskPos) const {
    return maskPos * impl_->zoom + impl_->pan;
}

// ========================================
// 操作
// ========================================

void RotoMaskEditor::deleteSelectedVertex() {
    if (!impl_->mask || impl_->selection.type != SelectionType::Vertex) return;
    
    impl_->saveUndoState();
    
    auto ids = impl_->mask->vertexIDs();
    if (impl_->selection.vertexIndex >= 0 && 
        impl_->selection.vertexIndex < static_cast<int>(ids.size())) {
        impl_->mask->removeVertex(ids[impl_->selection.vertexIndex]);
    }
    
    clearSelection();
    Q_EMIT maskChanged();
    update();
}

void RotoMaskEditor::clearAllVertices() {
    if (!impl_->mask) return;
    
    impl_->saveUndoState();
    impl_->mask->clearVertices();
    clearSelection();
    Q_EMIT maskChanged();
    update();
}

void RotoMaskEditor::undo() {
    if (impl_->undoStack.empty()) return;
    
    if (impl_->mask) {
        impl_->redoStack.push_back(impl_->mask->clone());
    }
    
    *impl_->mask = impl_->undoStack.back();
    impl_->undoStack.pop_back();
    
    Q_EMIT maskChanged();
    update();
}

void RotoMaskEditor::redo() {
    if (impl_->redoStack.empty()) return;
    
    if (impl_->mask) {
        impl_->undoStack.push_back(impl_->mask->clone());
    }
    
    *impl_->mask = impl_->redoStack.back();
    impl_->redoStack.pop_back();
    
    Q_EMIT maskChanged();
    update();
}

// ========================================
// 描画
// ========================================

void RotoMaskEditor::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 背景
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    // グリッド
    if (impl_->showGrid) {
        drawGrid(painter);
    }
    
    // パス
    drawPath(painter);
    
    // 制御点
    drawTangents(painter);
    
    // 頂点
    drawVertices(painter);
    
    // 選択ハイライト
    drawSelection(painter);
}

void RotoMaskEditor::drawGrid(QPainter& painter) {
    painter.setPen(QPen(impl_->gridColor, 1));
    
    double scaledGrid = impl_->gridSize * impl_->zoom;
    
    // 縦線
    double startX = std::fmod(impl_->pan.x(), scaledGrid);
    for (double x = startX; x < width(); x += scaledGrid) {
        painter.drawLine(static_cast<int>(x), 0, static_cast<int>(x), height());
    }
    
    // 横線
    double startY = std::fmod(impl_->pan.y(), scaledGrid);
    for (double y = startY; y < height(); y += scaledGrid) {
        painter.drawLine(0, static_cast<int>(y), width(), static_cast<int>(y));
    }
}

void RotoMaskEditor::drawPath(QPainter& painter) {
    if (!impl_->mask) return;
    
    auto vertices = impl_->getCurrentVertices();
    if (vertices.empty()) return;
    
    // パスを構築
    QPainterPath path;
    auto screenPos = maskToScreen(vertices[0].position);
    path.moveTo(screenPos);
    
    int n = static_cast<int>(vertices.size());
    int segments = impl_->mask->isClosed() ? n : n - 1;
    
    for (int i = 0; i < segments; ++i) {
        const auto& v0 = vertices[i];
        const auto& v1 = vertices[(i + 1) % n];
        
        QPointF p0 = maskToScreen(v0.position);
        QPointF p1 = maskToScreen(v0.position + v0.outTangent);
        QPointF p2 = maskToScreen(v1.position + v1.inTangent);
        QPointF p3 = maskToScreen(v1.position);
        
        path.cubicTo(p1, p2, p3);
    }
    
    painter.setPen(QPen(impl_->pathColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void RotoMaskEditor::drawVertices(QPainter& painter) {
    if (!impl_->mask) return;
    
    auto vertices = impl_->getCurrentVertices();
    auto ids = impl_->mask->vertexIDs();
    
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        QPointF screenPos = maskToScreen(vertices[i].position);
        
        bool isSelected = (impl_->selection.type == SelectionType::Vertex && 
                          impl_->selection.vertexIndex == i);
        
        QColor color = isSelected ? impl_->selectedVertexColor : impl_->vertexColor;
        
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(QBrush(color));
        painter.drawEllipse(screenPos, 6, 6);
    }
}

void RotoMaskEditor::drawTangents(QPainter& painter) {
    if (!impl_->mask) return;
    
    auto vertices = impl_->getCurrentVertices();
    
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        const auto& v = vertices[i];
        QPointF vertexScreen = maskToScreen(v.position);
        
        // 入力タンジェント
        if (!v.inTangent.isNull()) {
            QPointF inScreen = maskToScreen(v.position + v.inTangent);
            painter.setPen(QPen(impl_->tangentLineColor, 1, Qt::DashLine));
            painter.drawLine(vertexScreen, inScreen);
            
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QBrush(impl_->tangentColor));
            painter.drawEllipse(inScreen, 4, 4);
        }
        
        // 出力タンジェント
        if (!v.outTangent.isNull()) {
            QPointF outScreen = maskToScreen(v.position + v.outTangent);
            painter.setPen(QPen(impl_->tangentLineColor, 1, Qt::DashLine));
            painter.drawLine(vertexScreen, outScreen);
            
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QBrush(impl_->tangentColor));
            painter.drawEllipse(outScreen, 4, 4);
        }
    }
}

void RotoMaskEditor::drawSelection(QPainter& painter) {
    // 選択頂点のハイライト（すでにdrawVerticesで処理）
    Q_UNUSED(painter);
}

// ========================================
// マウスイベント
// ========================================

void RotoMaskEditor::mousePressEvent(QMouseEvent* event) {
    QPointF maskPos = screenToMask(event->position());
    
    switch (impl_->editMode) {
        case RotoEditMode::Draw: {
            if (!impl_->mask) break;
            
            impl_->saveUndoState();
            
            // 新しい頂点を追加
            int id = impl_->mask->addVertex(maskPos);
            impl_->mask->setPositionInterpolation(id, impl_->defaultInterpolation);
            
            // 前の頂点があれば、その出力タンジェントを自動設定
            auto ids = impl_->mask->vertexIDs();
            if (ids.size() >= 2) {
                int prevId = ids[ids.size() - 2];
                QPointF prevPos = impl_->mask->vertexPosition(prevId, impl_->currentTime);
                QPointF dir = maskPos - prevPos;
                double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
                if (len > 0.1) {
                    dir /= len;
                    impl_->mask->setOutTangent(prevId, dir * len * 0.3, impl_->currentTime);
                    impl_->mask->setInTangent(id, -dir * len * 0.3, impl_->currentTime);
                }
            }
            
            selectVertex(static_cast<int>(ids.size()) - 1);
            Q_EMIT vertexAdded(id);
            Q_EMIT maskChanged();
            break;
        }
        
        case RotoEditMode::Select:
        case RotoEditMode::Edit: {
            int vertexIdx = hitTestVertex(event->position());
            if (vertexIdx >= 0) {
                selectVertex(vertexIdx);
                impl_->isDragging = true;
                impl_->dragStartPos = maskPos;
                
                if (impl_->mask) {
                    impl_->dragOriginalValue = impl_->mask->vertexPosition(
                        impl_->mask->vertexIDs()[vertexIdx], impl_->currentTime);
                }
            } else {
                clearSelection();
            }
            break;
        }
        
        case RotoEditMode::Delete: {
            int vertexIdx = hitTestVertex(event->position());
            if (vertexIdx >= 0) {
                impl_->selection.vertexIndex = vertexIdx;
                impl_->selection.type = SelectionType::Vertex;
                deleteSelectedVertex();
            }
            break;
        }
    }
    
    update();
}

void RotoMaskEditor::mouseMoveEvent(QMouseEvent* event) {
    if (!impl_->isDragging || !impl_->mask) {
        update();
        return;
    }
    
    if (impl_->selection.type != SelectionType::Vertex) {
        update();
        return;
    }
    
    QPointF maskPos = screenToMask(event->position());
    QPointF delta = maskPos - impl_->dragStartPos;
    QPointF newPos = impl_->dragOriginalValue + delta;
    
    impl_->saveUndoState();
    
    auto ids = impl_->mask->vertexIDs();
    if (impl_->selection.vertexIndex >= 0 && 
        impl_->selection.vertexIndex < static_cast<int>(ids.size())) {
        impl_->mask->setVertexPosition(ids[impl_->selection.vertexIndex], newPos, impl_->currentTime);
    }
    
    Q_EMIT maskChanged();
    update();
}

void RotoMaskEditor::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    impl_->isDragging = false;
}

void RotoMaskEditor::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    // ダブルクリックでパスを閉じる
    if (impl_->mask) {
        impl_->mask->setClosed(true);
        Q_EMIT maskChanged();
        update();
    }
}

void RotoMaskEditor::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            deleteSelectedVertex();
            break;
            
        case Qt::Key_Escape:
            clearSelection();
            break;
            
        case Qt::Key_Z:
            if (event->modifiers() & Qt::ControlModifier) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    redo();
                } else {
                    undo();
                }
            }
            break;
            
        case Qt::Key_V:
            setEditMode(RotoEditMode::Select);
            break;
            
        case Qt::Key_P:
            setEditMode(RotoEditMode::Draw);
            break;
            
        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                // 全選択
                if (impl_->mask) {
                    // 実装簡略化
                }
            }
            break;
    }
    
    QWidget::keyPressEvent(event);
}

void RotoMaskEditor::wheelEvent(QWheelEvent* event) {
    double delta = event->angleDelta().y() / 120.0;
    double factor = std::pow(1.1, delta);
    
    QPointF mousePos = event->position();
    QPointF beforeZoom = screenToMask(mousePos);
    
    setZoom(zoom() * factor);
    
    QPointF afterZoom = screenToMask(mousePos);
    setPan(pan() + (afterZoom - beforeZoom) * zoom());
}

// ========================================
// ヒットテスト
// ========================================

SelectionType RotoMaskEditor::hitTest(const QPointF& screenPos, int& outVertexIndex) {
    outVertexIndex = hitTestVertex(screenPos);
    if (outVertexIndex >= 0) {
        return SelectionType::Vertex;
    }
    return SelectionType::None;
}

int RotoMaskEditor::hitTestVertex(const QPointF& screenPos, double threshold) {
    if (!impl_->mask) return -1;
    
    auto vertices = impl_->getCurrentVertices();
    
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        QPointF vertexScreen = maskToScreen(vertices[i].position);
        double dist = std::sqrt(
            std::pow(screenPos.x() - vertexScreen.x(), 2) +
            std::pow(screenPos.y() - vertexScreen.y(), 2)
        );
        
        if (dist < threshold) {
            return i;
        }
    }
    
    return -1;
}

int RotoMaskEditor::hitTestTangent(const QPointF& screenPos, bool isOutTangent, double threshold) {
    if (!impl_->mask) return -1;
    
    auto vertices = impl_->getCurrentVertices();
    
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i) {
        const auto& v = vertices[i];
        QPointF tangent = isOutTangent ? v.outTangent : v.inTangent;
        
        if (tangent.isNull()) continue;
        
        QPointF tangentScreen = maskToScreen(v.position + tangent);
        double dist = std::sqrt(
            std::pow(screenPos.x() - tangentScreen.x(), 2) +
            std::pow(screenPos.y() - tangentScreen.y(), 2)
        );
        
        if (dist < threshold) {
            return i;
        }
    }
    
    return -1;
}

} // namespace ArtifactCore