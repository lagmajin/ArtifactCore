# NLE Core Service Design Memo

> 2026-04-09 作成

## 目的

`Sequence / Track / Clip` の typed model を、編集処理として扱う service 境界に落とす。

このメモでは、NLE Core に必要な処理を

- `SequenceEditor`
- `ClipResolver`
- `LinkingService`
- `ConformService`

の 4 つに分け、責務が混ざらないようにする。

---

## 設計原則

### 1. 変更系と参照系を分ける

- 変更系: `SequenceEditor`
- 参照系: `ClipResolver`
- 規則系: `LinkingService`
- 再解決系: `ConformService`

### 2. service は source of truth を持たない

service は state を直接保持するのではなく、repository / model / snapshot を操作する。

- sequence 自体を所有しない
- clip の生データを複製しすぎない
- cache は持ってもよいが、正本にはしない

### 3. pull 型を基本にする

必要な値は必要時に取りに行く。

- source range
- resolved media
- linked group state
- conform 結果

### 4. 変更は command 化する

編集処理は undo / redo と接続できるよう、command として表現する。

- insert
- trim
- ripple
- roll
- slip
- slide
- replace

---

## SequenceEditor

### 役割

`SequenceEditor` は、sequence 構造に対する編集の入口。

### 主な責務

- track / clip / marker の追加・削除・移動
- insert / overwrite / trim / ripple / roll / slip / slide
- selection と linked rule を考慮した編集適用
- undo / redo 用 command 生成

### 持たないもの

- source の実体解決
- proxy 切り替え
- file I/O
- render backend

### 推奨 API

```cpp
class SequenceEditor {
public:
    EditResult insertClip(const SequenceId& sequenceId,
                          const TrackId& trackId,
                          const ClipDraft& draft);

    EditResult trimClip(const ClipId& clipId,
                        const FrameRange& newSourceRange,
                        TrimMode mode);

    EditResult rippleDelete(const ClipId& clipId);
    EditResult rollTrim(const ClipId& leftClipId,
                        const ClipId& rightClipId,
                        const FramePosition& boundary);
};
```

### 重要ルール

- ripple / linked editing は compound command として扱う
- 変更前後の snapshot を取得できるようにする
- sequence duration の再計算は service 内に閉じ込める

---

## ClipResolver

### 役割

`ClipResolver` は、clip が何を指していて、実際にどの media / source を使うかを解決する。

### 主な責務

- `SourceId` から source を解決する
- proxy / original の切り替え
- sourceRange と playbackRange の解決
- missing / offline / broken reference の診断

### 持たないもの

- sequence の編集規則
- linked selection の伝播
- track の順序制御

### 解決結果

```cpp
struct ClipResolution {
    ClipId clipId;
    SourceRef source;
    FrameRange sourceRange;
    FrameRange effectiveRange;
    bool useProxy = false;
    bool online = true;
    QString diagnostic;
};
```

### 重要ルール

- 解決不能なら typed な失敗結果を返す
- 解決結果は cache してよいが、source change で無効化する
- editor は resolver の詳細を知らない

---

## LinkingService

### 役割

`LinkingService` は、clip / track / selection の連動規則を扱う。

### 主な責務

- audio / video の linked group 管理
- selection link / trim link / move link の適用
- linked state の伝播
- link 切断時の状態保存

### 持たないもの

- source 解決
- sequence の構造編集
- conform 判定

### 推奨ルール

- link は明示的な group として持つ
- link の種類は複数に分ける
  - selection link
  - trim link
  - move link
  - visibility link
- 連動の有効 / 無効は state として保存する

### 重要ルール

- 連動は暗黙にしない
- link が壊れた場合は、編集を止めるより warning を返す
- UI は link の結果だけを読む

---

## ConformService

### 役割

`ConformService` は、source の変化やメタデータ差分に対して sequence を再解決する。

### 主な責務

- relink
- missing source の復旧候補探索
- source timebase 差の吸収
- proxy と original の整合
- nested sequence の再解決

### 持たないもの

- 通常の編集操作
- clip レイアウトの主導制御
- linked selection の直接処理

### 重要ルール

- conform は編集ではなく再解決
- 元の構造を壊さずに差分を当てる
- 変更内容は明示的な report として返す

### 推奨出力

```cpp
struct ConformReport {
    bool success = false;
    QVector<ClipId> updatedClips;
    QVector<ClipId> unresolvedClips;
    QVector<QString> warnings;
};
```

---

## 連携順序

### 標準フロー

1. `SequenceEditor` が編集要求を受ける
2. 必要に応じて `LinkingService` が連動規則を適用する
3. `ClipResolver` が必要な source / media を解決する
4. source 変化がある場合のみ `ConformService` が再解決する

### 逆流を避ける

- resolver が editor を呼ばない
- conform が link policy を直接変更しない
- editor が file system を直接触らない

---

## エラー方針

- typed result を返す
- warning と error を分ける
- 失敗でも可能な限り partial result を返す
- UI 向け説明文は service 側で組み立てられるようにする

推奨エラー例:

- invalid clip range
- overlapping edit
- missing source
- broken link group
- unsupported conform

---

## 実装フェーズ案

### Phase 1: Editor Skeleton

- `SequenceEditor` の最小 command API を作る
- insert / trim のみを先に通す

### Phase 2: Resolver

- source / proxy / missing の解決を分ける
- `ClipResolution` を固定する

### Phase 3: Linking

- linked group と selection link を実装する
- ripple / trim の連動を通す

### Phase 4: Conform

- relink / missing / timebase mismatch を再解決する
- report を返せるようにする

### Phase 5: Integration

- snapshot / undo / serialization と接続する
- playback / export からも同じ service を参照できるようにする

---

## Related

- `ArtifactCore/docs/MILESTONE_NLE_CORE_2026-04-09.md`
- `ArtifactCore/docs/MILESTONE_NLE_CORE_TYPE_DESIGN_2026-04-09.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`

## Current Status

2026-04-09 時点では設計メモ段階。
NLE の編集処理を service に分け、将来の UI や workflow が変わっても core の責務がぶれないようにする。
