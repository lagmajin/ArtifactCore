# マイルストーン: Typed Background Task Runtime

> 2026-04-09 作成

## 目的

`render / proxy / cache warm / analysis` を、共通の typed background task runtime で扱えるようにする。

個別の worker thread 実装を増やすのではなく、優先度・キャンセル・依存関係・進捗を共通化して、Core の非同期処理の見通しを良くする。

---

## 背景

Core にはすでに複数の非同期処理がある。

- render queue
- preview / cache warm
- proxy 生成
- analysis / waveform / AI 系処理

しかし、これらは実装単位ごとのバラつきがあり、

- cancel の扱い
- priority の扱い
- progress の報告方法
- dependency の管理

が揃っていない。

---

## 方針

### 原則

1. 低レベル thread 管理を各所に増やさない
2. task の状態は typed に扱う
3. progress / cancel / error を共通化する
4. job queue と worker pool を分離する
5. UI は task そのものではなく state を見る

---

## Phase 1: Task Contract

### 目的

task の共通契約を定義する。

### 作業項目

- task id / category / priority / state
- progress snapshot
- cancel token
- dependency list
- error payload

### 完了条件

- task の状態が typed に読める
- cancel と progress が共通 API で扱える

---

## Phase 2: Worker Pool

### 目的

複数の task を安全に回す。

### 作業項目

- worker pool
- concurrency limit
- task scheduling policy
- result dispatch

### 完了条件

- task を並列実行できる
- backend ごとの上限を設定できる

---

## Phase 3: Core Integration

### 目的

既存の background 処理を runtime に寄せる。

### 作業項目

- render queue integration
- proxy generation integration
- cache warm integration
- analysis integration

### 完了条件

- 既存の重い非同期処理が共通 runtime を使える
- UI 側が task state を追いやすい

---

## Non-Goals

- 完全な distributed job system
- すべての Core 処理の即時置換
- thread free の全面移行

---

## Related

- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/docs/MILESTONE_DETERMINISTIC_SNAPSHOT_CAPABILITY_CONTRACT_2026-04-09.md`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`

## Current Status

2026-04-09 時点では未着手。  
Core 側の非同期処理を共通化するための基盤 milestone として扱う。
