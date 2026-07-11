# Core-native Diagnostics Plan

**ステータス:** In Progress

## Purpose

`ArtifactCore` の Std／Qt置き換えを進める際に、AIと開発者が同じ構造化情報を読める診断経路を提供する。

## Implemented foundation

- `Utils.Result` に `SourceLocation` と `ErrorContext` を追加
- `Result<T>::errorContext()` と `Result<T>::fail(ErrorContext)` を追加
- `Result<T>::fail(code, message, operation, objectId, location)` の簡易生成を追加
- `Core.Diagnostics.Snapshot` にQt非依存の `DiagnosticEvent` / `DiagnosticSnapshot` を追加
- `Core.Diagnostics.Recorder` にスレッド安全なリングバッファを追加
- コンポーネント、対象ID、重大度、フレーム、スレッド、Trace ID、処理時間を保持
- Recorder発行の単調増加シーケンス番号でイベント順を保持
- `DiagnosticScope` 単位の自動Trace IDで処理イベントを相関付け
- Qt型を使わないJSON文字列出力を追加
- `snapshot` / `drain` / component・objectId・severity検索を追加
- `snapshotSince()` でカーソル以降の差分とリングバッファ欠落状態を同時取得
- 診断収集の有効／無効切り替えを追加
- `DiagnosticScope` によるRAII処理計測を追加
- FFmpegデコーダの `openFile` / seek処理へ接続
- 既存CrashHandlerのテキストレポートを読むQt非依存parserを追加
- Crashレポートを `DiagnosticSnapshot` へ変換する関数を追加
- Crashログ読込からSnapshot化までの一括APIを追加
- Qt Loggerへ既存ログ経路を使って表示する `appendDiagnostic` Adapterを追加
- Snapshot単位をQt Loggerへ転送する `appendDiagnostics` Adapterを追加
- 明示的な `Logger::flushDiagnostics` でRecorderから既存Loggerへflush可能
- `DiagnosticRecorder::recordResult()` で失敗Resultを共通イベントへ変換可能
- `tools/check_core_native_boundary.ps1` で新規Core-native module（診断・文字列・Encoding）のQt include/import・型名混入を検査
- `tools/check_core_diagnostics_contract.ps1` で必須APIと実接続点を検査
- `tools/check_core_contracts.ps1` で境界・診断・文字列契約を一括検査
- `tools/check_core_module_hygiene.ps1` でmodule purview後のinclude混入を検査

## Usage policy

```cpp
auto result = loadAsset(path);
if (!result) {
  DiagnosticRecorder::instance().recordResult(
      result, "AssetLoader");
}
```

処理単位は `DiagnosticScope` で囲み、成功時だけ `finish(true)` を呼ぶ。明示的に完了しなかったスコープは失敗として記録する。

差分監視では、読み取り後の `latestSequence()` をカーソルとして保存し、次回に
`snapshotSince(cursor, component, objectId)` を呼ぶ。返却Snapshotの
`eventsTruncated` が `true` の場合は、リングバッファの上限を超えて未取得イベントが
欠落しているため、AI解析では「完全な履歴ではない」と扱う。

## Qt boundary

Core-native診断APIでは `QString`、`QDateTime`、`QVector`、`QHash`、`QObject` を使用しない。Qt Logger／Traceへの表示変換はArtifact側のAdapterに閉じ込める。

## Next work

1. Qt LoggerからCore `DiagnosticEvent` へのAdapter
2. CrashHandlerは例外フィルタ内でRecorderを直接呼ばず、クラッシュレポートを起動後にAdapterで取り込む
3. `ProjectDiagnostic` のCore-native値型化
4. `check_core_qt_boundary` による公開moduleの依存検査
5. 既存 `qWarning()` の高価値経路から段階移行
6. 既存の個別 `.Test.cppm` 配置に合わせたRecorder／parserの自己検証ケース追加
