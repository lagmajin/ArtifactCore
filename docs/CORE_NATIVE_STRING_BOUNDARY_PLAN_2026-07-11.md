# Core-native String Boundary Plan

**ステータス:** In Progress

## Goal

既存の `QString` 中心の文字列ヘルパーを、Core-native処理とQt表示処理へ分離する。

## Boundary rules

- Coreの公開APIは `std::string_view`、`std::u8string_view`、`std::u16string_view` を基本にする
- 所有が必要なCore値は `std::string` または明示的なUTF-8 value typeを使う
- `QString` 変換は `ArtifactQtAdapter` 側に閉じ込める
- 暗黙の `QString`／`std::string` 変換を公開APIへ追加しない
- UTF-8の妥当性検証と変換失敗を `Result<T>` で返す

## Migration order

1. Encoding contract（UTF-8検証、BOM、code point数）
2. Core-native `StringView` / split / join / trim
3. Safe numeric parsing with `ErrorContext`
4. Path value and normalization contract
5. Qt Adapter（QString／QStringView／QRegularExpression）
6. Call-site migration by subsystem

## Compatibility policy

既存のQString APIは直ちに削除しない。新APIを追加し、境界Adapterへ移行した呼び出し元からQt依存検査の対象へ移す。

## AI-friendly diagnostics

文字列変換に失敗した場合は、`ErrorContext` に以下を必ず入れる。

- operation（例: `fromUtf8Checked`）
- objectId（入力元またはファイルID）
- code（`encoding.invalid_utf8` 等）
- source location

## Next slice

最初の実装単位は、既存 `StringConvertor` と `UniString::operator std::string()` の重複を調査し、UTF-8 checked conversionへ置き換える。

`UniString::operator std::string()` のUTF-16 code unit直列化バグは、UTF-8変換へ先行修正済み。

既存の `StringConvertor` / `StringView` は `QString` / `QStringView` を受け取るQt境界Adapterとして残し、Core-native APIからは依存しない。戻り値の互換性が必要な既存呼び出しを一括変更せず、呼び出し元の移行時にchecked conversionへ置き換える。

Core-native `isValidUtf8` / `fromUtf8Checked` / `toUtf32Checked` と、正常系・不正系の自己検証ケースを追加済み。

`detectBom` / `stripBom` / `fromUtf8BomAware` により、UTF-8／UTF-16／UTF-32 BOMの検出とUTF-8 BOM除去もCore側で扱える。

Core-native `splitView` / `join` も追加し、非所有の分割結果と所有文字列の結合を分離した。

Core-native `parseInt64` / `parseUInt64` / `parseBool` を追加し、不正値を `ErrorContext` 付きの `Result<T>` で返す。暗黙のfalse化は行わない。

Core-native `normalizePathSeparators` / `isAbsolutePath` / `hasParentTraversal` / `isSafeRelativePath` を追加し、QtやファイルI/Oに依存しないパス境界の前処理を開始した。NULを含む入力は明示的に拒否する。

相対アセットIDの受け入れ例:

```cpp
auto normalized = normalizePathSeparators(input, assetId);
if (!normalized) {
  DiagnosticRecorder::instance().recordResult(normalized, "AssetPath");
} else if (!isSafeRelativePath(normalized.value())) {
  DiagnosticRecorder::instance().record(
      CoreDiagnosticSeverity::Error, "path.unsafe_relative", "unsafe asset path",
      "AssetPath", "validate", assetId);
}
```
