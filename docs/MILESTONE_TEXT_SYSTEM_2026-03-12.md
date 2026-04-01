# Text System Milestone

After Effects レベルを目指す text 機能のための Core 側マイルストーン。

## C-TXT-1 Text Style Foundation

- 目標:
  font / fill / paragraph の基礎データを `ArtifactCore` で共通化する。
- 対象:
  `TextStyle`, `ParagraphStyle`, alignment, tracking, leading, casing。
- 完了条件:
  - UI layer が ad-hoc な `fontSize/fontFamily/color` の個別保持をやめられる
  - serialization と property panel の受け皿になる

## C-TXT-1A Font Foundation

- 目標:
  font family の解決と fallback を Core 側へ寄せる。
- 対象:
  `FontManager`, available families, default sans/mono, fallback resolution。
- 完了条件:
  - UI layer が直接 `QFontDatabase` を叩かなくても family 解決できる
  - missing font 時の fallback が統一される

## C-TXT-2 Layer Integration

- 目標:
  `ArtifactTextLayer` が Core の style を使って描画と property を持つ。
- 対象:
  font family, size, fill, alignment, tracking, leading, bold/italic, all caps。

## C-TXT-3 Paragraph / Box Text

- 目標:
  point text と box text を分けて扱えるようにする。
- 対象:
  text box size, auto-size, wrapping, paragraph alignment, vertical alignment。

## C-TXT-4 Typography Quality

- 目標:
  kerning, baseline shift, justification, ligature 相当の段階導入。
- 対象:
  glyph layout, line break policy, line metrics。

## C-TXT-5 Animator Bridge

- 目標:
  per-character / per-word / per-line animator の基盤を作る。
- 対象:
  selector, range, transform, opacity, fill/stroke animator。

## C-TXT-6 GPU Text Rendering / Japanese Shaping

- 目標:
  DX12 / Vulkan backend で日本語を含む text を安定描画できるようにする。
- 対象:
  font fallback, shaping, glyph atlas, backend parity, IME safe path。
- 詳細:
  `MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`

## First Pass Notes

- 2026-03-12:
  - Core に `Text.Style` module を追加
  - `ArtifactTextLayer` は最小の style/paragraph だけ共有する方向へ移行開始
  - `Font.FreeFont` の空 stub を family resolution 基盤として更新
  - DX12 / Vulkan 向けの日本語 text rendering は別マイルストーンに分離して整理する
