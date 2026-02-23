module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <vector>
#include <functional>

export module Core.Mask.RotoMask;

import std;

export namespace ArtifactCore {

/// ベジェ制御点を含むマスク頂点
struct RotoVertex {
    QPointF position;     ///< アンカーポイント
    QPointF inTangent;    ///< 入力タンジェント（position相対）
    QPointF outTangent;   ///< 出力タンジェント（position相対）
    
    RotoVertex() = default;
    RotoVertex(const QPointF& pos) : position(pos), inTangent(0, 0), outTangent(0, 0) {}
    RotoVertex(const QPointF& pos, const QPointF& in, const QPointF& out)
        : position(pos), inTangent(in), outTangent(out) {}
};

/// マスク合成モード
enum class RotoMaskMode {
    Add,        ///< 加算（ユニオン）
    Subtract,   ///< 減算
    Intersect,  ///< 交差
    Difference  ///< 差分
};

/// ロトスコープマスク（アニメーション対応ベジェパスマスク）
/// 
/// 時間と共に形状が変化するマスクを定義。各頂点は個別にアニメーション可能。
class RotoMask {
public:
    /// 頂点ID型
    using VertexID = int;
    
    // コンストラクタ・デストラクタ
    RotoMask();
    ~RotoMask();
    RotoMask(const RotoMask& other);
    RotoMask& operator=(const RotoMask& other);
    RotoMask(RotoMask&& other) noexcept;
    RotoMask& operator=(RotoMask&& other) noexcept;
    
    // ========================================
    // 頂点操作
    // ========================================
    
    /// 新しい頂点を追加し、そのIDを返す
    VertexID addVertex(const QPointF& position);
    
    /// 制御点付きで頂点を追加
    VertexID addVertex(const QPointF& position, const QPointF& inTangent, const QPointF& outTangent);
    
    /// 頂点を削除
    void removeVertex(VertexID id);
    
    /// 全頂点をクリア
    void clearVertices();
    
    /// 頂点数を取得
    int vertexCount() const;
    
    /// 全頂点IDを取得
    std::vector<VertexID> vertexIDs() const;
    
    // ========================================
    // 頂点位置・制御点
    // ========================================
    
    /// 頂点の位置を設定（現在時刻にキーフレーム追加）
    void setVertexPosition(VertexID id, const QPointF& position, double time);
    
    /// 頂点の位置を取得
    QPointF vertexPosition(VertexID id, double time) const;
    
    /// 入力タンジェントを設定
    void setInTangent(VertexID id, const QPointF& tangent, double time);
    
    /// 入力タンジェントを取得
    QPointF inTangent(VertexID id, double time) const;
    
    /// 出力タンジェントを設定
    void setOutTangent(VertexID id, const QPointF& tangent, double time);
    
    /// 出力タンジェントを取得
    QPointF outTangent(VertexID id, double time) const;
    
    /// 指定時間の頂点データを取得
    RotoVertex getVertex(VertexID id, double time) const;
    
    /// 指定時間の全頂点リストを取得（パス描画用）
    std::vector<RotoVertex> sampleVertices(double time) const;
    
    // ========================================
    // パス属性
    // ========================================
    
    /// パスが閉じているか
    bool isClosed() const;
    void setClosed(bool closed);
    
    /// パス名
    QString name() const;
    void setName(const QString& name);
    
    // ========================================
    // マスク属性（アニメーション対応）
    // ========================================
    
    /// 合成モード
    RotoMaskMode mode() const;
    void setMode(RotoMaskMode mode);
    
    /// 不透明度を設定 (0.0 - 1.0)
    void setOpacity(float opacity, double time);
    
    /// 不透明度を取得
    float opacity(double time) const;
    
    /// フェザー量を設定（ピクセル）
    void setFeather(float feather, double time);
    
    /// フェザー量を取得
    float feather(double time) const;
    
    /// エッジの拡張/収縮を設定（ピクセル）
    void setExpansion(float expansion, double time);
    
    /// エッジの拡張/収縮を取得
    float expansion(double time) const;
    
    /// 反転フラグ
    bool isInverted() const;
    void setInverted(bool inverted);
    
    // ========================================
    // アニメーション
    // ========================================
    
    /// アニメーションが存在するか
    bool isAnimated() const;
    
    /// キーフレーム時刻のリストを取得（全頂点・属性を統合）
    std::vector<double> keyframeTimes() const;
    
    /// アニメーション時間範囲を取得
    double startTime() const;
    double endTime() const;
    
    /// 指定時間に全プロパティのキーフレームを追加
    void addKeyframe(double time);
    
    /// 指定時間のキーフレームを削除
    void removeKeyframe(double time);
    
    // ========================================
    // 補間設定
    // ========================================
    
    /// 補間タイプ
    enum class Interpolation {
        Linear,     ///< 線形
        Bezier,     ///< ベジェ（滑らか）
        Step        ///< ステップ（ホールド）
    };
    
    /// 頂点位置の補間タイプを設定
    void setPositionInterpolation(VertexID id, Interpolation interp);
    Interpolation positionInterpolation(VertexID id) const;
    
    // ========================================
    // ラスタライズ
    // ========================================
    
    /// 指定時間のマスクをラスタライズ
    /// 出力は single channel float (0.0-1.0)
    /// width, height: 出力サイズ
    /// outData: float* (幅×高さの配列)
    void rasterize(double time, int width, int height, float* outData) const;
    
    /// マスクのバウンディングボックスを取得
    QRectF boundingBox(double time) const;
    
    // ========================================
    // ユーティリティ
    // ========================================
    
    /// マスクのコピーを作成
    RotoMask clone() const;
    
    /// 他のマスクからキーフレームをコピー
    void copyKeyframesFrom(const RotoMask& other, double timeOffset = 0.0);
    
    /// マスクを反転
    void reverse();
    
private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore