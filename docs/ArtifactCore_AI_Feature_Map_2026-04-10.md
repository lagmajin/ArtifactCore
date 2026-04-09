# ArtifactCore AI Feature Map

最終更新日: 2026-04-10

このドキュメントは、Artifact の AI システムが
「何を知れるか」「何を読めるか」「今後どう拡張するか」を整理するための機能表です。

## 目的

- AI にまず読み取り専用の情報取得をさせる
- その後に、確認付きの操作ツールへ進める
- AI が見える機能と、実際に実行できる機能を分けて管理する

## 現在の構成

### 1. 文脈収集

- `ArtifactCore/src/AI/TieredAIManager.cppm`
  - `globalContext()` を持つ
  - 背景分析で `AIContext` の `projectSummary` を更新できる
- `Artifact/src/AI/AIClient.cppm`
  - 現在のプロジェクト状態を集めて `AIContext` に詰める
  - クラウド / ローカルのどちらにも同じ文脈を渡す

### 2. AI 文脈データ

- `ArtifactCore/include/AI/AIContext.ixx`
  - `projectSummary`
  - `activeCompositionId`
  - `selectedLayers`
  - `recentActions`
  - `userPrompt` / `systemPrompt`
  - JSON 化可能

### 3. 機能説明カタログ

- `ArtifactCore/src/AI/UIWidgetsDescriptions.cppm`
- `ArtifactCore/src/AI/MoreDescriptions.cppm`
- `ArtifactCore/src/AI/ExportImportDescriptions.cppm`
- `ArtifactCore/src/AI/RenderAudioColorDescriptions.cppm`
- `ArtifactCore/src/AI/LayerDescriptions.cppm`

これらは AI が「Artifact に何があるか」を知るための説明群で、
実行用ツールではなく、知識ベースとして働く。

## 今できる情報取得

以下のような質問に答えやすくするのが、今の最初のゴールです。

- コンポジションはいくつあるか
- レイヤーが 10 個以上あるコンポジションはいくつあるか
- アクティブなコンポジションは何か
- 選択中のレイヤーは何か
- コンポジションごとのレイヤー数 / エフェクト数 / 長さはどうなっているか

## 参照元の例

AI が読むための集計値は、主に次の情報から組み立てる。

- `Artifact/include/Project/ArtifactProjectService.ixx`
  - 現在のプロジェクト / コンポジションへのアクセス
- `Artifact/include/Project/ArtifactProjectStatistics.ixx`
  - コンポジション数、レイヤー数、エフェクト数の集計
- `Artifact/include/Layer/Abstract.ixx`
  - レイヤーの名前や識別子
- `Artifact/include/Composition/Abstract.ixx`
  - コンポジション名や識別子

## 推奨する段階的拡張

### Phase 1: Read-only snapshot

- プロジェクト概要を要約する
- 統計値を AIContext に載せる
- シンプルな質問に正答できるようにする

### Phase 2: Structured inspection tools

- `list_compositions`
- `get_active_composition`
- `get_layer_counts`
- `get_selected_layers`

### Phase 3: Confirmed write tools

- `add_layer`
- `rename_layer`
- `select_layer`
- `remove_layer`

## 実装方針

- 最初は情報取得に限定する
- 破壊的操作はまだ自動実行しない
- 実行系は、AI の返答よりも UI 側の確認を優先する

## 関連ドキュメント

- `ArtifactCore/docs/ArtifactCore_Feature_List.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/docs/MILESTONE_NLE_CORE_2026-04-09.md`
- `ArtifactCore/docs/MILESTONE_DETERMINISTIC_SNAPSHOT_CAPABILITY_CONTRACT_2026-04-09.md`

