# Track Matte Core Milestone

`Layer2D` の `MatteMode` を、単なる設定値ではなく実際の layer evaluation に接続するための Core milestone.

## Goal

- matte source layer を明示的に参照できるようにする
- `Alpha / AlphaInverted / Luminance / LuminanceInverted` を Core で評価する
- track matte の依存順と cycle を Core 側で管理する
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

Core には `MatteMode` と `Layer2D::matteMode()` は既にあるが、まだ「どの layer を matte に使うか」「どう評価するか」が未定義。
そのため、現状の `track matte mode (未定義)` は UI の問題ではなく、Core の依存モデル不足として扱う。

この milestone では、layer の前後関係と matte source を Core のデータモデルに持たせ、
render / playback / diagnostics から一貫して使える状態を作る。

## Phases

### Phase 1: Data Model

- matte target layer と matte source layer の関係を定義する
- source が missing のときの fallback を決める
- self-reference と cycle を検出できるようにする

### Phase 2: Evaluation

- `Alpha` と `Luminance` を評価できるようにする
- inverted mode を含める
- layer visibility / opacity / matte の適用順を固定する

### Phase 3: Serialization

- project file に matte relationship を保存する
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

- `MatteMode` と `Layer2D::matteMode()` は既に存在する
- ただし Core の evaluation path はまだ未実装
- まずは data model を固定してから、render 側の適用に進むのが安全
