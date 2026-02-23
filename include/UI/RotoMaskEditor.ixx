module;

#include <QWidget>
#include <QPointF>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <vector>
#include <functional>

export module Core.UI.RotoMaskEditor;

import std;
import Core.Mask.RotoMask;

export namespace ArtifactCore {

/// ロトスコープマスク編集モード
enum class RotoEditMode {
    Select,     ///< 選択・移動モード
    Draw,       ///< 描画モード（頂点追加）
    Edit,       ///< 制御点編集モード
    Delete      ///< 頂点削除モード
};

/// 選択された要素のタイプ
enum class SelectionType {
    None,       ///< 選択なし
    Vertex,     ///< 頂点が選択されている
    InTangent,  ///< 入力タンジェントが選択されている
    OutTangent, ///< 出力タンジェントが選択されている
    Path        ///< パス全体が選択されている
};

/// 選択情報
struct RotoSelection {
    SelectionType type = SelectionType::None;
    int vertexIndex = -1;   ///< 選択された頂点のインデックス
    int pathIndex = 0;      ///< パスインデックス（将来的に複数パス対応用）
};

/// ロトスコープマスク編集ウィジェット
/// 
/// ベジェパスの描画・編集を行うUIコンポーネント。
/// 頂点の追加、移動、制御点編集をサポート。
class RotoMaskEditor : public QWidget {
    Q_OBJECT

public:
    explicit RotoMaskEditor(QWidget* parent = nullptr);
    ~RotoMaskEditor() override;

    // ========================================
    // マスク設定
    // ========================================
    
    /// 編集対象のマスクを設定
    void setMask(RotoMask* mask);
    
    /// 現在の編集対象マスクを取得
    RotoMask* mask() const;
    
    /// 現在時刻を設定（アニメーション用）
    void setCurrentTime(double time);
    double currentTime() const;

    // ========================================
    // 編集モード
    // ========================================
    
    /// 編集モードを設定
    void setEditMode(RotoEditMode mode);
    RotoEditMode editMode() const;
    
    /// 補間タイプを設定（新規頂点用）
    void setDefaultInterpolation(RotoMask::Interpolation interp);

    // ========================================
    // 表示設定
    // ========================================
    
    /// グリッドを表示するか
    void setShowGrid(bool show);
    bool showGrid() const;
    
    /// グリッドサイズ
    void setGridSize(int size);
    int gridSize() const;
    
    /// ズームレベル
    void setZoom(double zoom);
    double zoom() const;
    
    /// パン（スクロール位置）
    void setPan(const QPointF& pan);
    QPointF pan() const;

    // ========================================
    // 選択
    // ========================================
    
    /// 現在の選択状態を取得
    RotoSelection selection() const;
    
    /// 選択をクリア
    void clearSelection();
    
    /// 指定頂点を選択
    void selectVertex(int vertexIndex);

    // ========================================
    // 座標変換
    // ========================================
    
    /// スクリーン座標からマスク座標へ変換
    QPointF screenToMask(const QPointF& screenPos) const;
    
    /// マスク座標からスクリーン座標へ変換
    QPointF maskToScreen(const QPointF& maskPos) const;

    // ========================================
    // 操作
    // ========================================
    
    /// 選択頂点を削除
    void deleteSelectedVertex();
    
    /// 全頂点をクリア
    void clearAllVertices();
    
    /// 元に戻す
    void undo();
    
    /// やり直し
    void redo();

signals:
    /// マスクが変更された
    void maskChanged();
    
    /// 選択が変更された
    void selectionChanged(const RotoSelection& selection);
    
    /// 頂点が追加された
    void vertexAdded(int vertexId);
    
    /// 頂点が削除された
    void vertexRemoved(int vertexId);
    
    /// 編集モードが変更された
    void editModeChanged(RotoEditMode mode);
    
    /// ズームが変更された
    void zoomChanged(double zoom);

protected:
    // ========================================
    // Qtイベント
    // ========================================
    
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    class Impl;
    Impl* impl_;
    
    // 描画ヘルパー
    void drawGrid(QPainter& painter);
    void drawPath(QPainter& painter);
    void drawVertices(QPainter& painter);
    void drawTangents(QPainter& painter);
    void drawSelection(QPainter& painter);
    
    // ヒットテスト
    SelectionType hitTest(const QPointF& screenPos, int& outVertexIndex);
    int hitTestVertex(const QPointF& screenPos, double threshold = 10.0);
    int hitTestTangent(const QPointF& screenPos, bool isOutTangent, double threshold = 8.0);
};

} // namespace ArtifactCore