# MILESTONE: Text Shaping Backend / HarfBuzz

**Date**: 2026-06-12

---

## Purpose

`ArtifactCore` の text shaping を、`Qt QTextLayout` 依存の単一路線から、
`Qt` 互換経路と `HarfBuzz` 正規経路を切り替えられる構成へ進める。

目的は、Arabic, Hebrew, Indic scripts, Hangul, emoji ZWJ, ligature-heavy typography,
vertical writing の前提を壊さずに扱える shaping contract を作ること。

関連する共通意味論は [`MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md`](./MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md) に切り出す。

---

## Why

現状の text stack は、CJK を中心に描画できる土台はあるが、
complex script を正面から扱うにはまだ弱い。

特に次の点が課題になる。

- bidi resolved run を明示的に持ちにくい
- grapheme cluster と glyph cluster の境界が曖昧
- source text と visual order を混同しやすい
- vertical writing, ruby, tate-chu-yoko, kinsoku を同じ contract で扱いにくい
- editor と preview で shaping の責務がぶれる

HarfBuzz を導入すると、これらを `layout engine` 側へ寄せやすくなる。

---

## Goal

- shaping backend を差し替え可能にする
- `Qt` 互換経路を fallback として残す
- `HarfBuzz` を complex script の正規経路として扱う
- text animator は `shaped result` の上に乗せる
- vertical writing や ruby の前提を layout contract に載せる

## Execution Slice

- まずは [`MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_EXECUTION_2026-06-12.md`](./MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_EXECUTION_2026-06-12.md) から着手する
- 実行順は `Contract First → Qt Adapter → HarfBuzz Adapter → Consumer Wiring`
- 先に contract を固めてから shaping backend を入れる
- contract の詳細は [`MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md`](./MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md) を参照する

---

## Non-Goals

- すぐに既存の `QTextLayout` を全面削除すること
- HarfBuzz だけで UI まで完結させること
- glyph atlas や GPU instance をこの段階で完成させること
- 高度な typographic rule を一度に全部実装すること

---

## Proposed Architecture

### Shaping Backend Interface

text layout の前段を、次のような抽象に寄せる。

- input text run
- font selection
- writing mode
- base direction
- locale/script hints
- shaping result

### Output Contract

backend が返すものは glyph の単なる列ではなく、次を含む。

- visual glyph order
- logical to visual map
- cluster map
- advances
- offsets
- bidi runs
- direction info
- vertical orientation info
- punctuation adjustments
- ruby / tate-chu-yoko attachments

### Backend Variants

- `QtShapingBackend`
- `HarfBuzzShapingBackend`

`QtShapingBackend` は既存経路の互換層として残す。
`HarfBuzzShapingBackend` は complex script / vertical writing の正規経路にする。

---

## Suggested API Shape

```cpp
struct TextShapingRequest {
  QString text;
  TextStyle style;
  ParagraphStyle paragraph;
  TextWritingMode writingMode;
  TextDirection baseDirection;
  QString locale;
};

struct TextShapingResult {
  std::vector<GlyphItem> glyphs;
  QVector<int> logicalToVisual;
  QVector<int> visualToLogical;
  QVector<TextClusterSpan> clusters;
  QVector<TextBidiRun> bidiRuns;
  QVector<TextRubyAttachment> rubyAttachments;
  QVector<TextTateChuYokoRun> tateChuYokoRuns;
  QVector<TextPunctuationRun> punctuationRuns;
};

class ITextShapingBackend {
public:
  virtual ~ITextShapingBackend() = default;
  virtual TextShapingResult shape(const TextShapingRequest& request) = 0;
};
```

The exact type names can be adjusted, but the contract boundaries should stay.

---

## Integration Order

### 1. Contract First

- define request / result types
- define writing mode and base direction fields
- define cluster and run metadata

### 2. Qt Adapter

- wrap the existing `QTextLayout` path
- keep current behavior stable
- use it as fallback and comparison baseline

### 3. HarfBuzz Adapter

- integrate HarfBuzz shaping for complex scripts
- preserve logical/visual mapping
- support script- and direction-aware runs

### 4. Layout Consumers

- `TextLayoutEngine`
- `ArtifactTextLayer`
- `Text Animator`
- ruby / tate-chu-yoko / kinsoku consumers

---

## Vertical Writing Notes

HarfBuzz is not the whole solution for vertical writing, but it is the right shaping layer to host the metadata.

The backend contract should be able to carry:

- vertical glyph orientation
- alternate forms
- punctuation rotation policy
- tate-chu-yoko runs
- ruby attachments
- base direction and bidi runs

---

## Risks

- HarfBuzz integration adds a new dependency and a new code path to maintain
- bidi / vertical / ruby bugs can become subtle if the contract is weak
- existing `QTextLayout` behavior can regress if the fallback path is not kept
- glyph atlas and GPU backend work can get ahead of shaping semantics if sequencing is wrong

---

## Success Criteria

- `Qt` and `HarfBuzz` shaping backends can be swapped
- complex scripts render with stable logical/visual mapping
- vertical writing metadata is preserved through shaping
- `Text Animator` can consume shaped runs without caring which backend produced them
- fallback behavior is documented and predictable
