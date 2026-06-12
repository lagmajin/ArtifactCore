# MILESTONE: Text Shaping Backend / HarfBuzz - Execution Slice

**Date**: 2026-06-12
**Source**: [`MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md`](./MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md)

---

## Purpose

この execution slice では、`HarfBuzz` 導入の可否を議論するのではなく、
`Qt` 互換経路を残したまま `shaping backend` を切り替え可能にする最小契約を固める。

最初の目標は、complex script と vertical writing に必要な metadata を、
`TextLayoutEngine` とその downstream consumer に通せる形へ整理すること。

共通の意味論は [`MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md`](./MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md) を前提にする。

### Current Progress

- `Text Layout Contract` を module として追加済み
- `TextShapingBackend` の interface と `Qt` fallback skeleton を追加済み
- `HarfBuzz` backend はまだ fallback 実装の段階
- `Qt` fallback で `cluster` / `bidi` / `writingMode` を result contract に入れるところまで完了
- `ArtifactTextLayer` の glyph 生成を backend request 経由へ接続開始
- `ArtifactTextLayer` から `baseDirection` を request に流し始めた
- `ArtifactTextLayer` に `text.writingMode` を追加し、縦書きの入口を public surface に出した
- `DiligentImmediateSubmitter` の glyph text も shaping request 経由へ寄せ始めた
- `TextLayoutContract` に bracket / kinsoku の metadata container を追加した
- `Qt` fallback に vertical writing の粗い column flow を追加し始めた
- `TextLayoutEngine` の backend overload が writingMode / baseDirection を受け取れるようになった
- `TextLayoutContract` の vertical path で tate-chu-yoko run を返し始めた
- `ArtifactTextLayer` に ruby attachment の入口を追加し、request/contract に流し始めた
- `Qt` fallback の vertical path で ruby glyph overlay を描き始めた
- `Qt` fallback の vertical path で punctuation / bracket の upright-or-rotate を少し分け始めた
- `Qt` fallback の vertical path で minimal kinsoku line-start guard を入れ始めた

---

## Recommended Start Order

### 1. Contract Extraction

最初に shaping の入出力を固定する。

- request と result を分ける
- logical / visual mapping を持つ
- cluster / bidi / direction を明示する
- writing mode を contract に含める
- ruby / tate-chu-yoko / kinsoku の attachment contract を追加する

狙い:
- backend の実装より先に、上位層が何を期待するかを固定する
- `Qt` と `HarfBuzz` の差を contract の差に閉じる

### 2. Qt Adapter

次に、今の `QTextLayout` 経路を adapter 化する。

- 既存の挙動を壊さない
- complex script に不足する情報を明示的に埋める
- まずは fallback と比較基準を確保する

狙い:
- 既存の text 表示を維持しながら新契約へ寄せる
- 移行途中の regression を見つけやすくする

### 3. HarfBuzz Adapter

その後で `HarfBuzz` 実装を追加する。

- Arabic / Hebrew / Indic scripts の shaping を扱う
- bidi run と cluster map を保持する
- vertical orientation / punctuation / ruby metadata を通す

狙い:
- complex script を正規経路へ寄せる
- `Qt` 依存を fallback として残しつつ段階導入する

### 4. Consumer Wiring

最後に consumer 側を差し替える。

- `TextLayoutEngine`
- `ArtifactTextLayer`
- `Text Animator`
- selector / inspector / debug surface

狙い:
- shaping backend の差が UI へ漏れないようにする
- animator は shaped result の上に乗るだけにする

---

## Phase Boundaries

### In Scope

- backend interface の抽出
- `Qt` adapter の導入
- `HarfBuzz` adapter の導入
- result metadata の整理
- complex script と vertical writing の contract 保持

### Out of Scope

- glyph atlas の本実装
- GPU instance rendering
- ルビや禁則の UI 完成
- text animator の新機能追加

---

## Suggested Implementation Order

1. `TextShapingRequest` と `TextShapingResult` を固める
2. `QtShapingBackend` を作って現行経路を包む
3. `HarfBuzzShapingBackend` を追加する
4. `TextLayoutEngine` を backend switch に対応させる
5. `ArtifactTextLayer` で `writingMode` と `baseDirection` を流す
6. selector / inspector / debug で unit kind を見える化する
7. ruby / tate-chu-yoko / kinsoku metadata を consumer へ伝える

---

## Success Criteria

- `Qt` と `HarfBuzz` の切替が contract で説明できる
- complex script の shape 情報が lossless に近い形で流れる
- vertical writing metadata が consumer まで残る
- 既存の text 表示が fallback として維持される
- 次の workstream で ruby / tate-chu-yoko / kinsoku に進みやすい
