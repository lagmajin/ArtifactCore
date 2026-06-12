# MILESTONE: Text Layout Contract

**Date**: 2026-06-12

---

## Purpose

`Text Animator`, `complex script` 対応, `vertical writing`, `ruby`, `kinsoku`, `GPU text` を
同じ土台に乗せるための layout contract を定義する。

この文書の役割は、shaping backend の種類に依存しない共通の意味論を固定すること。

---

## Core Idea

text を「文字列」ではなく、次のレイヤーに分けて扱う。

- source text run
- script run
- bidi run
- grapheme cluster
- glyph cluster
- line run
- ruby attachment
- tate-chu-yoko run
- punctuation adjustment run
- kinsoku break candidate

---

## Proposed Types

### Writing Mode

```cpp
enum class TextWritingMode {
  Horizontal,
  Vertical,
};
```

### Direction

```cpp
enum class TextDirection {
  Auto,
  LeftToRight,
  RightToLeft,
};
```

### Script Run

```cpp
struct TextScriptRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString scriptTag;
  TextDirection direction = TextDirection::Auto;
  bool isComplexScript = false;
};
```

### Cluster Span

```cpp
struct TextClusterSpan {
  int logicalStart = 0;
  int logicalLength = 0;
  int visualStart = 0;
  int visualLength = 0;
  QString clusterId;
  QString scriptTag;
  bool isLigature = false;
  bool isEmojiSequence = false;
};
```

### Bidi Run

```cpp
struct TextBidiRun {
  int logicalStart = 0;
  int logicalLength = 0;
  TextDirection direction = TextDirection::LeftToRight;
  int visualOrder = 0;
};
```

### Line Run

```cpp
struct TextLineRun {
  int logicalStart = 0;
  int logicalLength = 0;
  int visualOrder = 0;
  int lineIndex = 0;
  bool isVerticalColumn = false;
};
```

### Ruby Attachment

```cpp
struct TextRubyAttachment {
  int baseLogicalStart = 0;
  int baseLogicalLength = 0;
  QString rubyText;
  float rubyScale = 0.5f;
  float rubyOffset = 0.0f;
};
```

### Tate-Chu-Yoko Run

```cpp
struct TextTateChuYokoRun {
  int logicalStart = 0;
  int logicalLength = 0;
  int maxInlineGlyphs = 4;
};
```

### Punctuation Run

```cpp
struct TextPunctuationRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString kind;
  bool hangingAllowed = false;
  bool rotateInVertical = true;
};
```

### Bracket Orientation Run

```cpp
struct TextBracketOrientationRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString bracketKind;
  bool rotateInVertical = true;
};
```

### Kinsoku Boundary Info

```cpp
struct TextKinsokuBoundaryInfo {
  int logicalStart = 0;
  int logicalLength = 0;
  bool breakBeforeAllowed = true;
  bool breakAfterAllowed = true;
};
```

---

## Invariants

- logical order と visual order を同時に保持する
- script run を selector と shaping の境界として保持する
- grapheme cluster を壊す selector は原則禁止する
- glyph cluster は shaping backend の結果として扱う
- vertical writing では punctuation / bracket / ruby / tate-chu-yoko の metadata を残す
- vertical writing では punctuation / bracket / ruby / tate-chu-yoko / kinsoku の metadata を残す
- source line / column run を selector debug に流せるようにする
- bidi 解決は layout contract で明示し、consumer が推測しない

---

## Consumers

- `TextLayoutEngine`
- `ArtifactTextLayer`
- `Text Animator`
- `ArtifactPropertyEditor`
- `ArtifactTimelineWidget`
- debug / inspector surface

---

## Success Criteria

- shaping backend を変えても consumer が見る contract が変わりすぎない
- vertical writing と complex script を同じデータモデルで扱える
- ruby, kinsoku, punctuation, tate-chu-yoko の entry point を統一できる
- selector semantics を `char` から解放できる
- backend から `cluster` / `bidi` / `writingMode` の初期情報を通せる
- `TextLayoutEngine` が backend overload を持ち、consumer が差し替え可能になる
