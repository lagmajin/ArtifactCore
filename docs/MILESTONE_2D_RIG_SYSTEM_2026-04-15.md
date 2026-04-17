# Milestone: Pro-Level 2D Rig & Bone System
**Date:** 2026-04-15  
**Target:** `ArtifactCore` + `Artifact` (Layer 型)  
**自己完結型** — ボーンデータ・IK ソルバー・コントローラーはすべて `ArtifactCore` に収める

---

## 概要 / Overview

After Effects の DUIK Ángeles / Joysticks 'n Sliders / RubberHose 相当の **プロレベル 2D リグシステム**を実装する。  
コアライブラリ側でリグデータ・ソルバー・ドライバーを自己完結させ、アプリ層は表示・編集 UI のみを担う。

### 既存資産（再利用）
| ファイル | 内容 | フェーズ |
|---|---|---|
| `ArtifactCore/include/Rig/Rig2D.ixx` + `src/Rig/Rig2D.cppm` | Bone2D 階層、Two-Bone IK、CCD IK | Phase 1 の土台 |
| `ArtifactCore/include/ImageProcessing/OpenCV/OpenCVPuppetEngine.ixx` | MLS/TPS/ARAP メッシュ変形 | Phase 3 に統合 |
| `ArtifactCore/include/Animation/AnimationDynamics.ixx` | Spring-Damper、Lag Follower | Phase 1 に組み込み |
| `ArtifactCore/include/Animation/AnimatableTransform3D.ixx` | RationalTime ベースのキーフレーム | Phase 1 のアニメ統合に使用 |

---

## Phase 1 — ボーン拡張・IK 強化・アニメーション統合
**目標:** 既存 `Rig2D` をプロダクション品質に引き上げる

### 1-A. `Rig2D` 拡張 (`Rig2D.ixx` / `Rig2D.cppm`)

#### ボーン制約 (`RigConstraint`)

```cpp
enum class ConstraintType {
    RotationLimit,  // 回転の最小・最大クランプ
    StretchLimit,   // 伸縮範囲（0.0〜∞ の倍率）
    LookAt,         // 常に指定オブジェクト方向を向く
    AimConstraint,  // ベクトル方向へ軸を一致させる
    Parent,         // 別ボーン/レイヤーに従属（オフセット付き）
    Orient,         // 回転のみ親に同期
};

struct RotationLimitConstraint {
    float minDeg = -180.0f;
    float maxDeg =  180.0f;
    bool  flipSide = false; // 膝の内側/外側切り替え
};

struct StretchConstraint {
    float minScale = 0.0f;   // 圧縮限界
    float maxScale = 2.0f;   // 伸長限界
    bool  volumePreserve = true; // 縦横逆スケールで体積保存
};
```

- `Bone2D` に `std::vector<RigConstraint> constraints_` を追加
- ソルバー後に `applyConstraints()` を呼ぶことで制約を反映

#### Rubber Hose / Stretchy Limb (DuIK 相当)

```cpp
// Bézier ベースの弾性肢変形
struct StretchyLimbDescriptor {
    float restLength;          // 自然長
    float stretchRatio;        // 現在の伸び率（1.0 = 自然長）
    float bend;                // 曲がりの強さ (-1〜+1)
    bool  rubberhoseStyle;     // Rubber Hose = true, Bone IK = false
};

// 弾性肢の中間点を Bézier でオフセット
QVector2D computeStretchyMidpoint(
    const QVector2D& rootPos,
    const QVector2D& tipPos,
    const StretchyLimbDescriptor& desc);
```

#### FABRIK IK ソルバー（多関節チェーン用）

```cpp
// Forward And Backward Reaching IK — 尻尾・脊椎・髪に必要
void solveFABRIK(
    QList<Bone2D*> chain,         // ルート→先端の順
    const QVector2D& target,
    int   iterations = 20,
    float tolerance  = 0.5f,
    Bone2D* poleTarget = nullptr  // 膝/肘の曲がり方向
);
```

- ポールターゲットは `Bone2D*` または外部 `QVector2D` で指定

#### Pole Vector 対応（TwoBoneIK 更新）

- `solveTwoBoneIK` に `poleVector: std::optional<QVector2D>` 引数を追加
- 曲げ方向を膝/肘ごとに制御可能にする

#### アニメーション統合

- `BoneTransform` の各チャンネル（position/rotation/scale）を `AnimatableValue<float>` でラップ
- `Bone2D::evaluate(const RationalTime& time) -> BoneTransform` を追加
- キーフレームは `AnimatableTransform3D` と同様の `RationalTime` スケールを使用
- スクラブ時に `AnimationDynamics::DynamicsChannel1D` を透過的に適用できる仕組み

---

## Phase 2 — コントローラー・ドライバーシステム (Joysticks 'n Sliders 相当)
**目標:** 1 本の UI コントロールで N 個のプロパティをバイリニア補間で動かす

### 新規ファイル構成

```
ArtifactCore/include/Rig/RigController2D.ixx
ArtifactCore/src/Rig/RigController2D.cppm
```

### `RigController2D` — 2D ジョイスティックコントローラー

```cpp
// 1 つのジョイスティック「点」の位置（例：中央/上/下/左/右/斜め45°）
struct ControllerKeyPoint {
    QVector2D position;  // コントローラー空間 (-1〜+1 の正規化座標)
    QString   label;
};

// このキー点での「各プロパティの値スナップショット」
struct ControllerPropertySnapshot {
    Id        propertyId;   // AnimatableValue を持つプロパティの ID
    QVariant  value;        // float / QVector2D / float angle 等
};

struct ControllerKeyFrame {
    ControllerKeyPoint               point;
    std::vector<ControllerPropertySnapshot> properties;
};

class RigController2D {
public:
    Id addKeyFrame(const ControllerKeyFrame& kf);
    void removeKeyFrame(const Id& kfId);
    void updateKeyFramePosition(const Id& kfId, const QVector2D& newPos);

    void setCurrentValue(const QVector2D& joystickPos);
    QVector2D currentValue() const;

    // バイリニア補間でプロパティ値を評価
    // 最寄り 3 または 4 個の KeyFrame からの距離で重み付け
    QVariant evaluateProperty(const Id& propertyId) const;

    // 全プロパティを一括評価
    std::vector<ControllerPropertySnapshot> evaluate() const;

    QJsonObject toJson() const;
    static RigController2D fromJson(const QJsonObject&);

private:
    std::vector<ControllerKeyFrame> keyFrames_;
    QVector2D currentValue_ = {0.0f, 0.0f};

    // IDW (逆距離加重) 補間 をデフォルト使用
    enum class InterpolationMethod { Barycentric, IDW };
    InterpolationMethod method_ = InterpolationMethod::IDW;
};
```

### `RigSlider` — 1D スライダーコントローラー

```cpp
// 0.0〜1.0 の 1D 値 → N プロパティを Bezier カーブで補間
// （表情コントロール、口の開き具合、眉の位置など）
class RigSlider {
public:
    void addBreakpoint(float t, std::vector<ControllerPropertySnapshot> props);
    void setCurrentValue(float t);
    std::vector<ControllerPropertySnapshot> evaluate() const;
    QJsonObject toJson() const;
    static RigSlider fromJson(const QJsonObject&);
private:
    std::vector<std::pair<float, std::vector<ControllerPropertySnapshot>>> breakpoints_;
    float currentValue_ = 0.0f;
};
```

### `RigAngle` — 角度コントローラー

```cpp
// ホイール/円形 UI → 0〜360° の角度で N プロパティを補間
// 目の回転、口角の上がり方など
class RigAngle {
public:
    void addBreakpoint(float angleDeg, std::vector<ControllerPropertySnapshot> props);
    void setCurrentAngle(float angleDeg);
    std::vector<ControllerPropertySnapshot> evaluate() const;
};
```

### ドライバーマップ (`RigDriverMap`)

```cpp
// ある bone の rotation(float) → 別 property への線形/カーブ変換
// 例: 腕の曲げ角度 → 袖のスケール変化
struct RigDriverBinding {
    Id    sourceBoneId;
    enum class SourceChannel {
        PositionX, PositionY, Rotation, ScaleX, ScaleY
    } sourceChannel;
    Id    targetPropertyId;
    float inputMin, inputMax;
    float outputMin, outputMax;
    std::optional<AnimatableCurve> remapCurve; // カスタムカーブリマップ
};

class RigDriverMap {
public:
    void addBinding(const RigDriverBinding& binding);
    void removeBinding(const Id& id);
    float evaluateBinding(const Id& bindingId, float sourceValue) const;
    void evaluateAll(const Rig2D& rig,
                     std::vector<ControllerPropertySnapshot>& out) const;
};
```

---

## Phase 3 — メッシュ変形とボーンスキニング統合
**目標:** OpenCVPuppetEngine を骨格データに連動させ、GPU メッシュ変形を実現

### ボーンスキンウェイト (`BoneSkinWeights`)

```cpp
struct VertexBoneInfluence {
    Id    boneId;
    float weight; // 0.0〜1.0
};

// 各頂点に対して最大 4 ボーンの影響ウェイト
using SkinWeightList = std::array<VertexBoneInfluence, 4>;

class BoneSkinWeights {
public:
    void setWeightForVertex(int vertexIndex, const SkinWeightList& weights);
    SkinWeightList weightForVertex(int vertexIndex) const;

    // 自動ウェイト計算: 最寄りボーン影響を距離ベースで算出
    void computeAutoWeights(const std::vector<QVector2D>& vertices,
                            const QList<Bone2D*>& bones,
                            int maxInfluences = 4);

    // GPU 転送用 float バッファへのフラット化
    std::vector<float> toGPUBuffer() const;
};
```

### GPU スキニングシェーダー (Diligent Compute Shader)

- 入力: Bind Pose 頂点バッファ + ボーン行列バッファ + スキンウェイトバッファ
- 出力: 変形後の頂点バッファ
- Pixel Shader でテクスチャサンプリング → 最終合成

### `RigSkinningEngine`

```cpp
class RigSkinningEngine {
public:
    explicit RigSkinningEngine(Diligent::IRenderDevice* device);
    ~RigSkinningEngine();

    void bind(const QImage& sourceImage,
              const BoneSkinWeights& weights,
              const Rig2D& bindPose);

    void deform(const Rig2D& currentPose);

    QImage resultImage() const;          // CPU fallback
    Diligent::ITexture* resultTexture(); // GPU パス用
};
```

---

## Phase 4 — アプリ層レイヤー型 (`Artifact` プロジェクト)
**目標:** リグ・コントローラーをコンポジション上で操作できる Layer として公開

### 新規レイヤー型

| クラス | モジュール名 | 役割 |
|---|---|---|
| `ArtifactBoneLayer` | `Artifact.Layer.Bone` | ビューポート上の 1 本のボーン |
| `ArtifactRigControllerLayer` | `Artifact.Layer.RigController` | ジョイスティック/スライダー UI |
| `ArtifactIKTargetLayer` | `Artifact.Layer.IKTarget` | IK エフェクタ・ポールターゲット |
| `ArtifactRigSkinLayer` | `Artifact.Layer.RigSkin` | スキニング変形対象レイヤー |

### `ArtifactBoneLayer` 概要

```cpp
class ArtifactBoneLayer : public ArtifactAbstractLayer {
public:
    ArtifactCore::Bone2D* bone();
    void setBoneLength(float length);
    bool isBoneVisible() const;

    // ビューポート描画: ボーン形状 (菱形 or 矢印) をギズモとして表示
    void drawGizmo(ArtifactIRenderer*, RationalTime);
};
```

### `ArtifactRigControllerLayer` 概要

```cpp
class ArtifactRigControllerLayer : public ArtifactAbstractLayer {
public:
    ArtifactCore::RigController2D& controller2D();
    ArtifactCore::RigSlider& slider();
    ArtifactCore::RigAngle& angle();

    enum class ControllerMode { Joystick2D, Slider1D, Angle360 };
    ControllerMode mode() const;
    void setMode(ControllerMode);

    // 評価: 現在値から全バウンドプロパティを更新
    void applyToScene(RationalTime);
};
```

### プロパティインスペクター統合

- `ArtifactBoneLayer` インスペクター: IK ソルバー選択、ポールターゲット指定
- `ArtifactRigControllerLayer` インスペクター: キー点の追加/削除、バインドプロパティ管理
- ジョイスティック UI: インスペクター内に 2D ポイント操作ウィジェット (120×120px)

---

## Phase 5 — 自動リグ・テンプレート・シリアライズ
**目標:** 作業効率を上げるテンプレートと保存/読み込み

### Auto-Rig テンプレート

```cpp
namespace RigTemplates {
    Rig2D bipedTemplate(float pixelHeight);    // 基本二足歩行
    Rig2D quadrupedTemplate(float pixelLength); // 四足歩行
    Rig2D faceTemplate(const QRectF& faceRegion); // 顔リグ
}
```

### シリアライズ

```cpp
QJsonObject Rig2D::toJson() const;
static Rig2D Rig2D::fromJson(const QJsonObject&);
QJsonObject RigController2D::toJson() const;
QJsonObject BoneSkinWeights::toJson() const;
```

---

## 実装順序まとめ

```
Phase 1 (優先)
  ├─ Bone2D: 制約システム (RotationLimit, StretchLimit)
  ├─ Rig2D: FABRIK IK + ポールベクター
  ├─ StretchyLimb (Rubber Hose) 実装
  └─ BoneTransform → AnimatableValue 統合

Phase 2
  ├─ RigController2D (ジョイスティック + IDW 補間)
  ├─ RigSlider
  ├─ RigAngle
  └─ RigDriverMap

Phase 3
  ├─ BoneSkinWeights (自動ウェイト計算)
  ├─ RigSkinningEngine (GPU skinning via Diligent)
  └─ OpenCVPuppetEngine との統合ブリッジ

Phase 4
  ├─ ArtifactBoneLayer
  ├─ ArtifactRigControllerLayer
  ├─ ArtifactIKTargetLayer
  └─ プロパティインスペクター UI

Phase 5
  ├─ Auto-rig テンプレート
  ├─ JSON シリアライズ
  └─ コピー/ペースト
```

---

## 設計上の決定事項

| 課題 | 決定 | 理由 |
|---|---|---|
| IK ソルバーの選択 | TwoBone + CCD + FABRIK の 3 種 | 用途別に使い分けるため |
| コントローラー補間 | IDW (逆距離加重) をデフォルト | 点数が不均等でも安定 |
| メッシュ変形バックエンド | CPU (OpenCV MLS/ARAP) + GPU (Diligent Compute) の 2 パス | CPU はプレビュー、GPU は書き出し |
| アニメ統合 | `AnimatableValue<float>` + `RationalTime` で既存 transform と統一 | 既存キーフレーム UI を再利用 |
| 自己完結性 | コアロジックはすべて `ArtifactCore` に収める | Artifact UI なしでも単体利用可能 |
| Qt 依存 | `Rig2D` は Qt (QVector2D/QMatrix4x4) を使用 | 2D canvas 座標との親和性が高い |

---

## 参照すべき既存実装

- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx` — キーフレームパターン
- `Artifact/src/Widgets/Render/TransformGizmo.cppm` — ギズモ描画パターン
- `ArtifactCore/include/Animation/AnimationDynamics.ixx` — ダイナミクス統合パターン
- `ArtifactCore/include/ImageProcessing/OpenCV/OpenCVPuppetEngine.ixx` — Phase 3 の前提
- `ArtifactCore/include/Rig/Rig2D.ixx` — Phase 1 の起点
