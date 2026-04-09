# NLE Core Type Design Memo

> 2026-04-09 作成

## 目的

`MILESTONE_NLE_CORE_2026-04-09.md` で定義した NLE Core Foundation を、実装しやすい typed data model に落とす。

このメモでは特に `Sequence / Track / Clip` のデータ構造を固定し、あとから AE 寄りの composition model と混ざらないようにする。

---

## 設計原則

### 1. 時間と構造を分ける

- 構造: `Sequence`, `Track`, `Clip`
- 時間: `FrameRange`, `FramePosition`, `FrameRate`, `TimeBase`
- 参照: `Source`, `Media`, `Proxy`, `Conform`

構造体の責務を混ぜず、時間計算は専用の value object に寄せる。

### 2. 参照は ID ベースにする

直接ポインタ参照を前提にせず、保存可能な ID を基準にする。

- `SequenceId`
- `TrackId`
- `ClipId`
- `MarkerId`
- `SourceId`
- `TransitionId`

### 3. 位置情報は絶対と相対を両方持つ

NLE では source 側と timeline 側の両方が必要になる。

- source 上の区間
- sequence 上の配置
- 必要なら trim 前後の元範囲

### 4. 派生値は必要時に計算する

pull 型を前提にする。

- duration
- overlap
- gap
- linked span
- effective playback range

これらは保存の主対象にしない。

---

## コア型

### TimeBase

タイムベースを明示する。

```cpp
struct TimeBase {
    int32_t numerator = 1;
    int32_t denominator = 30;
    bool dropFrame = false;

    double fps() const;
    bool isValid() const;
};
```

用途:

- sequence time
- source time
- timecode 解決
- rate change の基準

### SequenceId / TrackId / ClipId

```cpp
struct SequenceId {
    quint64 value = 0;
};

struct TrackId {
    quint64 value = 0;
};

struct ClipId {
    quint64 value = 0;
};
```

用途:

- serialization
- undo / redo
- linked selection
- cross-reference

### SourceRef

素材参照は解決対象とメタデータを分ける。

```cpp
struct SourceRef {
    SourceId sourceId;
    QString uri;
    QString displayName;
    QString checksum;
    bool online = true;
    bool proxyAvailable = false;
};
```

用途:

- media library
- relink
- proxy swap
- missing source 診断

---

## Sequence

### 概要

`Sequence` は NLE の編集空間の中心。

### 推奨フィールド

```cpp
struct Sequence {
    SequenceId id;
    QString name;
    TimeBase timeBase;
    FrameRange duration;

    QVector<TrackId> trackOrder;
    QVector<MarkerId> markers;

    TrackLayout defaultLayout;
    SequenceFlags flags;
};
```

### 責務

- track の順序管理
- timebase の保持
- 全体 duration の管理
- marker / layout / flags の保持

### 持たないもの

- clip の実体
- source の直接解決ロジック
- render backend の詳細
- UI 状態

---

## Track

### 概要

`Track` は sequence 内の時間レーン。

### 推奨フィールド

```cpp
enum class TrackKind {
    Video,
    Audio,
    NestedSequence,
    Adjustment,
    Subtitle,
    Data
};

struct Track {
    TrackId id;
    SequenceId ownerSequenceId;
    TrackKind kind = TrackKind::Video;

    QString name;
    int32_t order = 0;

    bool enabled = true;
    bool locked = false;
    bool solo = false;
    bool mute = false;

    QVector<ClipId> clipOrder;
    QVector<TransitionId> transitions;
};
```

### 責務

- clip の並びを保持する
- track 単位の enabled / locked / solo / mute を持つ
- track kind に応じて表示・出力の意味を変える

### 持たないもの

- clip の source 範囲
- track 外の編集規則
- project 全体の source registry

---

## Clip

### 概要

`Clip` は source を timeline に配置した最小編集単位。

### 推奨フィールド

```cpp
struct Clip {
    ClipId id;
    TrackId trackId;
    SourceId sourceId;

    FrameRange sourceRange;
    FrameRange timelineRange;
    FrameRange trimRange;

    double speed = 1.0;
    double opacity = 1.0;

    bool enabled = true;
    bool locked = false;
    bool reversed = false;

    quint64 linkedGroupId = 0;
    QVector<TransitionId> attachedTransitions;
    QVector<MarkerId> markers;
};
```

### 責務

- source のどの区間を使うかを保持する
- sequence 上の配置と長さを保持する
- speed / reverse / opacity などの clip-level modifier を持つ
- linked group を通じて他 clip と関係を持てるようにする

### 持たないもの

- source 実体そのもの
- track 全体の順序
- sequence 全体の timebase
- render cache の実体

---

## 補助型

### Marker

```cpp
struct Marker {
    MarkerId id;
    SequenceId sequenceId;
    FramePosition position;
    QString name;
    QString note;
    QColor color;
};
```

### Transition

```cpp
struct Transition {
    TransitionId id;
    TrackId trackId;
    ClipId leftClipId;
    ClipId rightClipId;
    FrameRange range;
    TransitionKind kind = TransitionKind::Crossfade;
    double duration = 12.0;
};
```

### Clip Link Group

```cpp
struct ClipLinkGroup {
    quint64 id = 0;
    QVector<ClipId> members;
    bool videoAudioLinked = true;
    bool selectionLinked = true;
    bool trimLinked = true;
};
```

---

## 派生計算

### Sequence 系

- total duration
- active track count
- visible track list
- marker query

### Track 系

- clip gaps
- overlapping clip detection
- track occupancy
- enabled clip count

### Clip 系

- effective playback range
- source time mapping
- trim result
- linked ripple impact

これらは cache してよいが、source of truth にはしない。

---

## 編集規則メモ

### Insert / Overwrite

- insert: 後続を押し出す
- overwrite: 既存配置を上書きする

### Ripple / Roll

- ripple: 総尺を変えながら後続を連鎖移動する
- roll: 境界を動かして総尺を維持する

### Slip / Slide

- slip: timeline 位置を維持し source 範囲を動かす
- slide: 前後の配置関係を保ちながら移動する

### Nested Sequence

- sequence を clip として配置できる
- 参照循環は禁止
- child sequence の timebase は明示的に解決する

---

## Serialization 方針

- versioned schema を持つ
- unknown field は保持できる余地を残す
- ID と参照先を分けて保存する
- derived value は再計算可能なら保存しない

推奨の保存単位:

- `Sequence`
- `Track`
- `Clip`
- `Marker`
- `Transition`
- `SourceRef`
- `ClipLinkGroup`

---

## C++ 実装メモ

### 1. まず value object を切る

- `TimeBase`
- `SequenceId` / `TrackId` / `ClipId`
- `SourceRef`
- `FrameRange` の NLE 利用整理

### 2. 次に entity を切る

- `Sequence`
- `Track`
- `Clip`
- `Marker`
- `Transition`

### 3. 最後に service を切る

- `SequenceEditor`
- `ClipResolver`
- `LinkingService`
- `ConformService`

---

## Related

- `ArtifactCore/docs/MILESTONE_NLE_CORE_2026-04-09.md`
- `ArtifactCore/docs/MILESTONE_NLE_CORE_SERVICE_DESIGN_2026-04-09.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`
- `ArtifactCore/docs/FrameRange_Usage.md`
- `ArtifactCore/docs/PlaybackClock_Usage.md`

## Current Status

2026-04-09 時点では設計メモ段階。
`Sequence / Track / Clip` の型境界を固定することで、後続の実装とテストのブレを減らす。
