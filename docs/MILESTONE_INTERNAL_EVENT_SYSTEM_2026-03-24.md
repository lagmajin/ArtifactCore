# Internal Event System Milestone

Core 内部の高速イベント配信基盤を整備するためのマイルストーン。
Qt UI スレッド連携やバックグラウンド処理の同期点を、Core 非依存の形で扱えるようにする。

## Goal

- typed なイベント配信を Core に置く
- 即時配信と queued 配信を分ける
- UI 層は Qt 依存のブリッジに閉じ込める
- スレッド境界をまたいでも壊れにくい購読・解除モデルを持つ

## Design Rules

- Core 側は Qt に依存しない
- イベント本体はテンプレート/型付きで扱う
- 購読解除は RAII で安全に行う
- queued delivery は `drain()` で明示的に消費する
- 既存の UI 更新は、まず Core の event bus に集約し、必要な箇所だけ UI thread bridge に流す

## DoD

- `EventBus` が typed subscription を提供する
- `publish()` が即時配信できる
- `post()` が後で `drain()` できる
- subscription の解除が二重解放せずに安全に動く
- Qt 依存のない状態で単体利用できる

## Work Packages

### E-1 Typed Bus Foundation

- `subscribe`
- `publish`
- `post`
- `drain`
- `clear`

### E-2 Subscription Safety

- RAII token
- move-only handle
- bus 破棄後でも安全に無効化

### E-3 UI Thread Bridge Policy

- UI 層側の bridge は別モジュールに置く
- Core の bus は thread-safe queue とする
- `drain()` の呼び出し主は UI / worker の両方を許容する

### E-4 Diagnostics / Stats

- subscriber count
- pending queue count
- 軽量な debug hook

## Non-Goals

- Qt の signal / slot 置き換え
- reflection ベースの自動配信
- distributed event system
- 永続イベントログ

## Follow-up Notes

- 2026-03-24:
  - まずは Core 側に typed bus の骨組みを置く
  - その後で `Artifact` 側に Qt bridge を必要最小限で追加する
