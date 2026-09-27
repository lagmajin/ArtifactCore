# MILESTONE: Text Shaping Backend / HarfBuzz - Execution Slice

**Date**: 2026-06-12
**最終更新**: 2026-09-27
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
- `TextLayoutContract` に `scriptRuns` を追加し、連続 script 区間の metadata を流し始めた
- `ArtifactTextLayer` へ `scriptRuns` の summary 表示をつなぎ始めた
- `ArtifactTextLayer` へ vertical metadata の件数 summary も追加し始めた
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

### HarfBuzz backend 実装（2026-09-27）

`HarfBuzzShapingBackend` を Qt fallback スタブから実動作する実装へ置き換えた。
利用側（`ArtifactTextLayer` / `DiligentImmediateSubmitter`）はまだ `QtShapingBackend` を
直接指名しているため、**既存の描画挙動は変化していない**。

実装内容:

- `vcpkg.json` に `harfbuzz` を直接依存として追加。`ArtifactCore/CMakeLists.txt` で
  `find_package(harfbuzz CONFIG REQUIRED)` + `harfbuzz::harfbuzz` をリンク
- `FontManager::fontFileBytes()` を追加。`QFont` からディスク上の sfnt bytes を解決する。
  Qt 6 には `QFontDatabase::findFontFile()` が無く、`QRawFont::fontTable()` は
  単一 OpenType テーブルのみを返すため、application font の family→path 対応表と
  OS フォントディレクトリ走査で組み立てる
- `TextShapingBackend.cppm` に `hb-ft` 経路を追加。`FT_Library` は `thread_local` で
  1 度だけ初期化し、`FT_Face` は family+style+size を key とした cache で使い回す
- `hb_buffer_add_utf32` + `hb_buffer_guess_segment_properties()` で shaping。
  `info.cluster` は UTF-32 インデックスなので既存の `buildContract()` /
  `makeIdentityResult()` の codepoint 基準とそのまま整合する
- script / direction は明示指定がある場合のみ上書きし、無指定は HarfBuzz 推測に委ねる

残件:

- 縦書き・ruby・tate-chu-yoko は Qt fallback のまま
- 実機 DX12 / Vulkan 受入と backend parity は未検証

### 多言語対応（2026-09-27）

ICU 78.2 を正式リンクし、script property と bidi を実データに置き換えた。

**① script property**

`scriptTagForCodepoint()`（旧 `:97-163`）は手書きの範囲表で ISO 15924 の約12%しか
検出せず、**未検出の文字を全て `Latn` と名乗っていた**。この値は
`contract.scriptRuns` に入り Text Animator のセレクタとインスペクタまで伝播していた。

ICU `uscript_getScript()` + `uscript_getShortName()` に置き換え、
約160 script を正確に判定するようになった。Hiragana / Katakana / Kanji も
正しい別 tag として区別される。

- **default を `Latn` にしない**: Common は `Zyyy`、Inherited は `Zinh` のまま返す。
  数字・記号・結合文字が「スクリプトを持たない」と名乗る
- `isComplexScriptTag()` を追加。`scriptRuns.isComplexScript` の判定は
  従来の「`Latn` 以外」判定から複雑 script の明示リスト判定に変更したので、
  数字や結合文字が complex と誤判定されなくなった
- `harfBuzzScriptFor()` は `hb_script_from_iso15924_tag()` を使い、
  `Latn` / `Zyyy` / `Zinh` は HarfBuzz の推測に委ねる

**④ bidi**

`logicalToVisual` / `visualToLogical` が恒等写像で `bidiRuns` が常に1本だったのを、
ICU `ubidi_*`（UAX #9 準拠）に置き換えた。

- `ubidi_setPara()` に `UBIDI_DEFAULT_LTR` / `UBIDI_DEFAULT_RTL` を渡して
  P2/P3 の段落レベル判定も委譲する
- `ubidi_getVisualRun()` の visual run を `TextBidiRun` へ書き写すので、
  混在方向の行が複数 run に分割される
- run 境界は UTF-16 単位で返るため、code point インデックスへ変換してから contract へ入れる
- codepoint → glyph の写像を通して glyph 単位の `logicalToVisual` / `visualToLogical` も作る
- ICU が失敗した場合は identity（従来挙動）にフォールバック

**③ OpenType feature と本番配線**

`shapeWithHarfBuzz()` に `hb_buffer_set_cluster_level(MONOTONE_GRAPHEMES)` と
`hb_feature_t` リスト（`kern` / `liga` / `calt` / `clig` / `locl`）を追加。
旧的 `hb_shape(font, buffer, nullptr, 0)` は HarfBuzz の script デフォルト feature
だけだった。

これらは HarfBuzz 経路にしか効かないため、production の4箇所も接続した:

- `ArtifactCore/src/Text/GlyphLayout.cppm`
- `Artifact/src/Layer/ArtifactTextLayer.cppm`（`shapeText` と rich text metadata）
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`

`QtShapingBackend` は HarfBuzz 経路の fallback としてのみ残る。

残件:

- **フォント fallback の script キー化**。`FreeFont.ixx` の
  `containsCjkCharacters() || containsEmojiCharacters()` gate が残っており、
  Devanagari / Thai / Arabic / Hebrew の glyph 欠落を検出しても fallback しない
- **Indic conjunct の完全対応**。`contract.clusters` の `visualLength = 1` は
  構造上の仮定が残る
- **縦書き**（`vert` / `vrt2`、`QFont::setVertical`、括弧の縦書き形）
- HarfBuzz 経路での複数行レイアウト（`boxWidth > 0` は Qt fallback のまま）
- `TextLayoutContract` の Qt 型（`QVector` / `QPointF` / `QRectF` / `QString`）置換
- 実機 DX12 / Vulkan 受入と backend parity は未検証

### 特殊言語対応（2026-09-27）

**Q1: combining mark と ZWJ シーケンスの消失を修正**

shaping までは正しかったが、**ラスタライズが結果を捨てていた**。
`PrimitiveRenderer2D` と `DiligentImmediateSubmitter` が `GlyphKey` を
`charCode` だけで作っていたため、`GlyphAtlas` は 1 codepoint の glyph を
引いており、U+0301 のような結合文字は glyph index 0（`.notdef`）になって
`acquire` が `valid=false` を返していた。絵文字 ZWJ シーケンスも最初の
codepoint だけになって複数 codepoint が別々に描かれていた。

- `ResolvedGlyphFont` に `shapedGlyphIndex` と `clusterText` を追加し、
  **キャッシュの同一性判定に含めた**（code point だけだと `fi` ligature の
  `f` と通常の `f` が衝突する）
- ハッシュには `shapedGlyphIndex` と `clusterText.size()` を使い、
  全文比較は衝突時のみ行う
- `resolvedGlyphFont()` を `GlyphItem` を受ける形に。codepoint から直接
  GlyphKey を作る `drawGlyphText` 経路のために `char32_t` 版オーバーロードを残す
- `renderModeForCodePoint()` による再推測をやめ、contract が決めた
  `glyph.renderMode` を使う（DirectWrite のカラー経路は
  `renderMode == ColorBitmap` で分岐するため、ここが合っていないと
  カラー絵文字が monochrome に落ちる）
- フォント fallback の問い合わせも `clusterText` 全体に対して行う

**Q2: locale を BCP-47 に**

`hb_buffer_set_language()` に `QLocale::system().name()`（Windows だと
`"tr_TR"`）を渡していた。HarfBuzz は BCP-47（`"tr-TR"`）を期待するので
`HB_LANGUAGE_INVALID` になり、トルコ語の点なし i が反映されていなかった。

- `toBcp47LanguageTag()` でアンダースコアをハイフンへ変換
- language だけでなく script も渡す（`locl` の選択には script が必要）
- `TextLayoutEngine::layout` が空文字を_locale にしていたのを
  `QLocale::system().name()` に変更し他経路と統一

**Q3: テキストのテストを新設**

`tests/ArtifactCore/TextShapingTest.cpp` を追加（従来は 0 件）。
`artifact_add_test(ArtifactCoreTextShapingTest)` で登録。

1. script: `Latn` / `Zyyy`（数字・記号）/ `Zinh`（結合文字）、
   主要 script の tag、**Hiragana/Katakana が `Hani` と区別される**、
   **Georgian / Ethiopic / Cherokee が `Latn` にならない**（回帰防止の中核）
2. bidi: 純 RTL で `baseDirection == RightToLeft`、混在行で
   `bidiRuns.size() >= 3`、RTL で `logicalToVisual` が恒等でない、
   純 LTR では恒等であること
3. cluster: 結合文字 / ZWJ family / skin tone / regional indicator が
   それぞれ 1 クラスタ
4. backend parity: 単一行は HarfBuzz が発火、折り返しは Qt へ fallback、
   縦書きは Qt へ fallback、complex script で HarfBuzz の glyph 数 ≥ Qt

HarfBuzz がフォント解決に失敗して Qt に落ちている場合に
`HarfBuzzHandlesUnwrappedSingleLine` が失敗するようにしてある。

残件:

- ligature / kerning の UI オフスイッチ（`TextStyle` に feature 欄が要る）
- NFC 正規化（ICU `unorm2`）。分解入力と合成入力はまだ別物
- 数学記号・音楽記号・点字の fallback
- ZWNJ の Qt 経路（`shapedGlyphByUtf16[sourceIndex] = 0`）
- 縦書きの `vert` / `vrt2` とルビの縦書き配置
- RTL ヒットテスト（`logicalToVisual` は実データになったので今後対応可能）

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
