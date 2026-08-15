# Matte Stack / Child Matte Core Milestone

**最終更新:** 2026-08-15

## Update 2026-08-15

- `MatteStack` / `MatteNode`、Add/Common/Subtract、JSON、`MatteEvaluator` と `Layer2D::matteStack()` を現行 Core で確認。
- Artifact 側では legacy `LayerMatteReference` が複数 matte、cycle 防止、missing diagnostics、render 適用、Timeline 操作の主経路として接続されている。
- Core `Layer2D::MatteStack` を全 render 経路の canonical source へ統合する作業は未完了。vector-based `evaluateMatteStack()` は alpha factor 入力を前提とし、luminance source の評価は `MatteEvaluator` / image 経路へ分散している。
- Core/Artifact のモデル統合、hidden source policy、実素材での Add/Common/Subtract・inverted luma parity は未検証。

`Layer2D` の matte を、AE 風に「隣接レイヤー」ではなく「レイヤーにぶら下がる子要素」として扱うための Core milestone.

## Goal

- matte を layer の child / attached node として扱う
- 複数 matte を `Add / Common / Subtract` で合成できるようにする
- `Alpha / AlphaInverted / Luminance / LuminanceInverted` を Core で評価する
- dependency order と cycle check を Core 側で管理する
- serialization / import / diagnostics を Core の責務として整える

## Scope

- `ArtifactCore/include/Layer/LayerMatte.ixx`
- `ArtifactCore/include/Layer/Layer2D.ixx`
- `ArtifactCore/src/Layer/Layer2D.cppm`
- `ArtifactCore/src/Layer/*` の matte evaluation path
- `ArtifactCore/src/ImageProcessing/*` の matte sampling 補助

## Non-Goals

- layer panel の UI 実装
- composition editor の操作導線
- mask / roto editor の入力 UI

## Background

Core には `MatteMode` と `Layer2D::matteMode()` は既にあるが、まだ「どの layer を matte に使うか」「複数 matte をどうまとめるか」が未定義。
そのため、現状の `track matte mode (未定義)` は UI の問題ではなく、Core の依存モデル不足として扱う。

この milestone では、layer の子要素として matte stack を持ち、render / playback / diagnostics から一貫して使える状態を作る。

## Proposed Model

- `Layer`
  - `content`
  - `children`
  - `matteStack`
- `MatteNode`
  - `source`
  - `mode`
  - `invert`
  - `enabled`
  - `order`
- `MatteStackMode`
  - `Add`
  - `Common`
  - `Subtract`

## Phases

### Phase 1: Data Model

- matte target layer と matte source layer の関係を定義する
- source が missing のときの fallback を決める
- self-reference と cycle を検出できるようにする
- child matte node の ownership を決める

### Phase 2: Evaluation

- `Alpha` と `Luminance` を評価できるようにする
- inverted mode を含める
- layer visibility / opacity / matte の適用順を固定する
- `Add / Common / Subtract` の合成規則を固定する

### Phase 3: Serialization

- project file に matte stack を保存する
- import / open / save で壊れないようにする
- backward compatibility のための default を決める

### Phase 4: Diagnostics

- missing matte source を health issue として報告する
- cycle / invalid mode / hidden source などを検出する
- debug string と health dashboard で追えるようにする

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

## Current Status

- `MatteStack`／`MatteNode`、Add／Common／Subtract、JSON、`MatteEvaluator`、`Layer2D::matteStack()` は Core に実装済み。
- Artifact 側の `LayerMatteReference` は複数 matte、cycle 防止、missing diagnostics、render 適用、Timeline／Inspector 操作へ接続済み。
- Core stack を全 render 経路の canonical source へ統合する作業、luminance source parity、hidden source policy、実素材での受入れは未完了または未検証。
- `MaskCutout` と `TimeRemap` は引き続き別ルートとして扱う。
