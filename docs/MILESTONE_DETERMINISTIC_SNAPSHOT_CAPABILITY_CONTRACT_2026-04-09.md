# マイルストーン: Deterministic Snapshot & Capability Contract

**最終更新:** 2026-09-14

> 2026-04-09 作成

## 目的

`ArtifactCore` の各処理を、frame snapshot と capability contract で扱えるようにする。

これにより、render / playback / export / AI / audio が shared mutable state に依存しにくくなり、再現性の高い処理と backend fallback を実装しやすくする。

---

## 背景

今の Core には、個別のサービスや helper は揃ってきている一方で、

- この frame に必要な入力 state は何か
- この backend が何を保証できるか
- この処理は再実行して同じ結果を期待できるか

が統一された契約としてはまだ弱い。

MFR、render queue、backend fallback、diagnostics を進めるには、この境界を先に固める価値が高い。

---

## 方針

### 原則

1. mutable state を snapshot へ寄せる
2. backend capability を typed で表す
3. frame / job / service の責務を混ぜない
4. fallback は contract 上で説明できるようにする
5. export / preview / playback で同じ snapshot を再利用できるようにする

---

## Phase 1: Snapshot Model

### 目的

処理入力を frame 単位で固定する。

### 作業項目

- composition snapshot
- playback snapshot
- render input snapshot
- export job snapshot
- AI / audio の補助 state snapshot

### 完了条件

- 1 frame の処理に必要な state をまとめて渡せる
- shared state の読み取り箇所が減る

---

## Phase 2: Capability Model

### 目的

backend の可能 / 不可 / 条件付き可を typed に扱う。

### 作業項目

- backend capability struct / enum を定義する
- encode / decode / readback / audio / analysis の capability を分離する
- fallback reason を列挙できるようにする

### 完了条件

- UI / diagnostics から capability が読める
- backend 切り替えの理由を説明できる

---

## Phase 3: Deterministic Execution Boundaries

### 目的

同じ snapshot から同じ結果を得やすくする。

### 作業項目

- cache key と frame dependency の整理
- mutable global の参照を減らす
- frame-local temp object の寿命を短くする
- render / export の実行境界を明示する

### 完了条件

- 再実行時の挙動差が減る
- cache / fallback の追跡がしやすい

---

## Non-Goals

- 全処理の完全 deterministic 化
- distributed execution
- hardware 差を完全に消すこと

---

## Related

- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/src/Playback/PlaybackClock.cppm`
- `ArtifactCore/src/Image/FFmpegEncoder.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

## Current Status

2026-09-14 時点で基盤の first slice を実装済み。

- `Graphics.RenderPipelineFoundation` に `RenderInputSnapshot`、typed backend capability、fallback reason、backend selection を追加
- snapshot の frame/time、resolution、color descriptor、quality、scene/render-index/settings revision を cache key へ反映
- resolved backend を cache key に含め、GPU と software の結果を混同しない契約を追加
- `RenderIndex::snapshot()` の proxy 順序を安定 ID 順に固定
- Core diagnostic contract test に snapshot、cache key、fallback、RenderIndex 順序の検証を追加

Composition evaluator からの snapshot 生成、Diligent resource ownership との接続、実 runtime での deterministic parity は未完了。
