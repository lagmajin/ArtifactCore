# MILESTONE: GPU Text Rendering / Japanese Shaping - Execution Slice

**Date**: 2026-04-30  
**Source**: [`MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`](./MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md)

---

## Purpose

この execution slice では、DX12 / Vulkan backend の text 表示を一気に完成させるのではなく、まず日本語が壊れにくい土台を固める。
最初に font resolution と shaping の基礎を揃え、その次に backend parity と atlas 系へ進む。

---

## Recommended Start Order

### 1. Font Resolution / Fallback

最初に fallback の安定化を確認する。

- CJK 文字列に対する fallback 順序を固定する
- missing glyph の扱いを統一する
- family / weight / italic の解決を backend 依存にしすぎない

狙い:
- まず「読める」状態を保証する
- backend をまたいでも同じ説明ができるようにする

### 2. Shaping / Layout

次に、文字列単位のレイアウトを詰める。

- glyph positioning を run 単位で確認する
- 長音、濁点、全角記号の扱いを整理する
- 将来の HarfBuzz 連携に向けて受け口を作る

狙い:
- 日本語の行組みを崩しにくくする
- text layer と composition preview で同じ基盤を使えるようにする

### 3. GPU Text Backend

fallback と shaping が揃ったら、GPU path を固める。

- glyph atlas の生成と再利用
- quad / vertex 配信の経路を確認
- SDF / bitmap / stroke のどれを core の第一段にするか固定する

狙い:
- CPU fallback だけに頼らない text 表示へ近づける
- 先に atlas の責務を切ることで、後続の backend work を分割しやすくする

### 4. Backend Parity

描画できるようになったら、backend 間の差を追う。

- DX12 / Vulkan / Diligent の見え方差分を比較する
- preview と render queue の差を記録する
- AA / hinting / upscale の違いを debug できるようにする

狙い:
- 差分をゼロにするのではなく、説明可能な範囲に収める
- 以後の text work の回帰検知をしやすくする

### 5. Editor / Layer Integration

最後に editor 側へ戻す。

- `ArtifactTextLayer` と inline edit を同じ基盤に寄せる
- IME / 日本語入力の扱いを確認する
- per-character animation と矛盾しないかを確認する

狙い:
- text rendering を editor に閉じず、layer / preview / input で共通化する

---

## Phase Boundaries

### In Scope

- `FontManager` の fallback ルール整理
- `GlyphLayout` / `TextStyle` の run 単位整備
- atlas と backend の責務分離
- preview と composition で同じ text 資産を使う方向の整理

### Out of Scope

- 新しい text animator の仕様追加
- 大規模な UI redesign
- video / image pipeline の色管理移行
- QPainter 完全排除の一気通貫化

---

## Suggested Implementation Order

1. CJK fallback の順序と warning を固定する
2. run 単位の shaping で崩れやすい文字列を洗う
3. glyph atlas の責務を最小単位で切る
4. backend parity の比較条件を文書化する
5. editor / layer の共通化ポイントを詰める

---

## Success Criteria

- 日本語が backend で読める
- fallback が予測可能
- shaping の差分を追える
- atlas の責務が明確
- preview と layer が同じ text 基盤に乗る

