# GPU Text Rendering / Japanese Shaping (2026-04-01)

DX12 / Vulkan 系の render backend で、日本語を含む text を安定して描画するためのマイルストーン。

現状の `TextStyle` / `GlyphLayout` / `FontManager` は、文字レイアウトの基礎としては存在するが、GPU backend での本格的な text rendering と shaping、CJK fallback、atlas 管理まではまだ担っていない。

## Goal

- DX12 / Vulkan backend でも日本語が破綻なく見えるようにする
- text layout を backend 非依存の共通基盤に寄せる
- glyph shaping と font fallback を Core 側で整理する
- render path の CPU / GPU 差分を text だけで増やしすぎない

## Current Progress

- locale-aware font fallback の初手を実装済み
- `FontManager` が CJK 系文字列を見た場合に日本語フォント候補を優先できるようになった
- `TextLayoutEngine` と `ArtifactTextLayer` は sample text を使って font resolve する方向へ寄せ始めた

## Definition Of Done

- 日本語テキストが DX12 / Vulkan backend で表示できる
- 長音、濁点、全角記号、ひらがな / カタカナ / 漢字が崩れにくい
- missing font 時に統一された fallback が働く
- font / size / weight / italic / tracking / leading が描画結果に反映される
- backend が変わっても text layer の見え方差分が説明可能な範囲に収まる

## Scope

- `ArtifactCore/include/Text/GlyphLayout.ixx`
- `ArtifactCore/include/Text/TextStyle.ixx`
- `ArtifactCore/include/Font/FreeFont.ixx`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/DiligentDeviceManager.cppm`

## Work Packages

### 1. Font Resolution / Fallback

- 目標:
  - 日本語フォントが使えない環境でも、落ちずに代替描画できるようにする
- 作業項目:
  - font family の存在確認と fallback を整理する
  - Japanese / CJK 向けの標準 fallback 順序を作る
  - missing glyph 時の警告を統一する
- 完了条件:
  - 文字化けではなく、少なくとも読める fallback が出る
- 進捗:
  - CJK 文字列を見たときの fallback family selection を導入済み

### 2. Shaping / Layout

- 目標:
  - 文字単体ではなく、連続した文字列として正しくレイアウトする
- 作業項目:
  - glyph positioning を text run 単位に寄せる
  - 日本語の全角記号や長音の扱いを整理する
  - 将来的な HarfBuzz 連携の受け口を作る
- 完了条件:
  - 基本的な日本語の行組みが崩れにくい

### 3. GPU Text Backend

- 目標:
  - DX12 / Vulkan の描画経路で text を出せるようにする
- 作業項目:
  - glyph atlas の生成と再利用
  - GPU への vertex / quad 配信
  - SDF / bitmap / stroke のいずれかを backend に載せる
- 完了条件:
  - CPU fallback だけに頼らず text が描画される

### 4. Backend Parity

- 目標:
  - Diligent / DX12 / Vulkan で text の見え方差分を把握する
- 作業項目:
  - アップスケール / AA / hinting 差分の比較
  - overlay と composition 本体で同じ text 資産を使う
  - preview と render queue で差が出る条件を記録する
- 完了条件:
  - backend 間の差分が debug 可能になる

### 5. Editor / Layer Integration

- 目標:
  - `ArtifactTextLayer` と editor の inline edit で同じ text 基盤を使う
- 作業項目:
  - property panel / inline edit / render preview の共通化
  - IME 入力と日本語入力の扱い整理
  - per-character animation の前提を保つ
- 完了条件:
  - 日本語入力からレンダリングまで破綻しない

## Risks

- DX12 / Vulkan backend での text は、単純な `QPainter` 依存を外すほど実装量が増える
- 日本語対応は fallback だけでは足りず、shaping と atlas の管理が必要になる
- backend 固有の見え方差分をゼロにするのは現実的でないため、許容差を先に定義する必要がある

## Related

- `ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md`
- `Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md`

---

## Execution Slice

- まずは [`MILESTONE_GPU_TEXT_RENDERING_JA_EXECUTION_2026-04-30.md`](./MILESTONE_GPU_TEXT_RENDERING_JA_EXECUTION_2026-04-30.md) から着手する
- 実行順は `Font Resolution / Fallback → Shaping / Layout → GPU Text Backend → Backend Parity → Editor / Layer Integration`
- 先に「読める」状態を固めてから、backend と editor の差を詰める
