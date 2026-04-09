# マイルストーン: NLE Core Foundation

> 2026-04-09 作成

## 目的

`ArtifactCore` に、AE 向けの composition 中心モデルとは別に、Premiere ライクな NLE を将来的に支えられる編集基盤を用意する。

ここで作るのは UI ではなく、`sequence / track / clip / transition / marker / conform / relink / proxy` のような編集コアである。
将来のアプリ形態が After Effects 寄りでも Premiere 寄りでも、共通で再利用できる時間モデルと編集操作を先に固める。

---

## 背景

今の `ArtifactCore` には、動画編集に必要な要素が少しずつ揃っている。

- `FrameRange` / `FramePosition` / `FrameRate`
- `PlaybackClock`
- `Video` / `Audio` 系の source や layer
- `FFmpegEncoder` / export 周辺
- snapshot / capability / background task の基盤

ただし、これらはまだ「composition を再生するための部品」に寄りやすく、
NLE で必要になる次の概念が一つの編集モデルとしてまとまっていない。

- clip の source time と timeline time の対応
- insert / overwrite / ripple / roll / slip / slide の編集規則
- linked audio/video の同期
- nested sequence や compound clip の扱い
- offline / proxy / relink / conform

将来的に Premiere ライクなアプリを作る可能性を残すなら、今のうちに NLE 側のコア境界を切っておく価値が高い。

---

## 方針

### 原則

1. composition モデルと sequence モデルを混ぜない
2. clip / track / sequence / media を typed な domain object として扱う
3. 編集操作は command 化して undo / redo と結びつける
4. source 管理、proxy、relink、conform を core の責務に含める
5. 再生・書き出し・プレビューは同じ snapshot / timebase を共有できるようにする
6. UI 実装は後回しにし、まずは core の意味論を固定する

### AI 実装条件

1. 1ファイル 1責務 を基本にする
2. 時間軸は `FrameRange` と `FramePosition` を軸にする
3. クリップ編集は副作用を明示する
4. linked selection や ripple の伝播はテストで固定できるようにする
5. render / export / playback の入力 state を分けて扱う

---

## 対象ドメイン

- **Project / Media**
  素材管理、メタデータ、参照先の解決
- **Sequence**
  NLE の編集中シーケンス本体
- **Track**
  video / audio / nested / adjustment 的なレーン
- **Clip**
  source range を timeline に配置した編集単位
- **Transition**
  clip 間の時間的な接続や crossfade
- **Marker**
  編集位置の参照点、コメント、検索キー
- **Conform**
  source 変化に対する再解決
- **Proxy**
  軽量素材と本素材の切り替え
- **Linking**
  audio/video の連動、選択の連動、編集の連動

---

## 推奨アーキテクチャ

### Sequence Graph

Sequence は track の集合として扱う。

- track は時間上の並びを持つ
- clip は source time / timeline time / duration を持つ
- nested sequence は clip と同じ見え方で扱える余地を残す
- transition は clip の境界にぶら下がるか、専用オブジェクトとして持つ

### Time Model

NLE は `frame` だけでなく、時間基準のズレを吸収できる必要がある。

- project timebase
- source timebase
- sequence timebase
- drop-frame / non-drop-frame timecode
- rate change / retime / speed ramp の入口

### Media Resolution

素材の解決は push ではなく pull に寄せる。

- 参照先が必要な時点で source を解決する
- offline / missing / proxy / relink を同じ契約で扱う
- 失敗理由を typed に返せるようにする

### Edit Operations

編集は command として定義する。

- insert / overwrite
- ripple trim
- roll trim
- slip / slide
- split / blade
- replace
- linked move / linked trim

### Rendering Contract

NLE の preview / export は、同じ編集状態から再現可能であるべき。

- render input snapshot
- audio mixdown snapshot
- cache key
- capability contract

---

## ライフサイクル

### Attach / Detach

1. media / clip / track を sequence に追加する
2. sequence が timebase と source 参照を保持する
3. remove 時は ripple / linked rule に応じて後続を調整する

### Enable / Disable

track / clip / transition は保持したまま無効化できる。
これは UI 上の mute / solo / disable / offline と相性が良い。

### Update

update は 2 系統に分ける。

- `timeline update`: 編集操作や scrub に対する離散更新
- `playback update`: 再生中の連続更新

---

## データモデル

### Clip Identity

各 clip は以下を持つ。

- `clipId`
- `sourceId`
- `sourceIn`
- `sourceOut`
- `timelineIn`
- `timelineOut`
- `enabled`
- `locked`
- `linkedGroupId`

### Track Identity

各 track は以下を持つ。

- `trackId`
- `trackType`
- `enabled`
- `locked`
- `solo`
- `mute`
- `order`

### Sequence Identity

各 sequence は以下を持つ。

- `sequenceId`
- `name`
- `timebase`
- `duration`
- `defaultTrackLayout`
- `markerSet`

---

## 編集規則

### Trim / Ripple / Roll

- trim は clip の境界を詰める
- ripple は後続の時間を連鎖的に押し出す
- roll は隣接 clip の境界を動かして総尺を維持する

### Slip / Slide

- slip は timeline 上の位置を維持したまま source 範囲を動かす
- slide は前後の配置関係を保ちながら位置を動かす

### Linked Editing

- audio/video の連動を切り替えられるようにする
- selecting / trimming / moving のリンク規則を分離する
- 連動解除時の状態を snapshot に残せるようにする

### Nested Sequence

- sequence を clip として扱える余地を残す
- 循環参照を避ける
- nested sequence の timebase は明示的に解決する

---

## 既存機能との接続

### Undo / Redo

- edit operation は command 化する
- ripple / linked operation は compound command として扱う
- clip state の snapshot を取れるようにする

### Serialization

- project save/load で sequence 構造を失わない
- offline / proxy / relink 状態を保存する
- version upgrade で clip schema を壊さない

### Playback / Export

- playback で見えている編集状態を export に流用できるようにする
- frame accurate な seek と scrub を前提にする
- audio sync を sequence time と整合させる

### Diagnostics

- missing media
- invalid time range
- overlapping clip
- orphan transition
- broken linked group

---

## 非目標

- NLE 専用 UI の完成
- Premiere / Resolve の完全再現
- 細かいエフェクト UI の全面実装
- collaborative editing
- distributed render

---

## 実装フェーズ案

### Phase 1: Temporal Domain

- `sequence / clip / track / marker` の型を定義する
- source time と timeline time の変換を固定する
- timebase と drop-frame の扱いを決める

### Phase 2: Editing Primitives

- insert / overwrite / ripple / roll / slip / slide を定義する
- split / replace / linked move を command 化する
- undo / redo と接続する

### Phase 3: Media Conform

- proxy / offline / relink / missing media を扱う
- metadata ベースの source 再解決を整える
- nested sequence を試験的に導入する

### Phase 4: Playback / Export Bridge

- sequence snapshot を render / playback / export で共有する
- audio mixdown と video render の入力を揃える
- cache key と capability contract を固める

### Phase 5: Stability

- linked selection / ripple の副作用を安定化する
- serialization と version migration を整える
- diagnostic を UI へ返しやすくする

---

## 検証ポイント

- clip 編集で source time と timeline time がずれない
- ripple / roll / slip / slide の結果が予測可能
- linked audio/video の編集が崩れない
- proxy / relink 後も sequence が復元できる
- nested sequence でも timebase が破綻しない
- export と playback で同じ編集状態を見られる

---

## 関連

- `ArtifactCore/docs/MILESTONE_NLE_CORE_TYPE_DESIGN_2026-04-09.md`
- `ArtifactCore/docs/MILESTONE_NLE_CORE_SERVICE_DESIGN_2026-04-09.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/docs/FrameRange_Usage.md`
- `ArtifactCore/docs/PlaybackClock_Usage.md`
- `ArtifactCore/docs/TimelineClock_Architecture.md`
- `ArtifactCore/src/Frame/FrameRange.cppm`
- `ArtifactCore/src/Playback/PlaybackClock.cppm`
- `ArtifactCore/src/Image/FFmpegEncoder.cppm`

## Current Status

2026-04-09 時点では未着手。
Premiere ライクな NLE を将来コア流用で作れるようにするための、時間軸と編集操作の基盤 milestone として扱う。
