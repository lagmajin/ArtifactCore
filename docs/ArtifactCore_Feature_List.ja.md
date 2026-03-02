# ArtifactCore 機能一覧

最終更新日: 2026-03-02

このドキュメントは、ArtifactCore ライブラリの機能を俯瞰するための高レベルなマップです。
開発者が主要システムを素早く見つけるための索引として利用します。

## コアアーキテクチャ

- C++20 モジュールベースのコアライブラリ
- `include/` 配下にドメイン指向モジュールを配置
- `src/` 配下に実行時実装を配置
- 再利用のため UI 非依存ロジックを優先

## 機能領域

### タイムラインと再生

- `Playback/PlaybackClock`: 高精度クロック、再生速度、ループ制御
- `Frame/*`: フレーム位置、範囲、レート、オフセット、時間
- `Time/*`: タイムコード、Duration、有理時間、リマップ補助
- `Media/MediaPlaybackController`: メディア向け再生制御

### 入力・キーマップシステム（Blender風基盤）

- `UI/InputOperator`: コンテキスト対応の入力処理
- `UI/KeyMap`: 設定可能なキーマップ定義とバインド
- `UI/InputEvent`: 正規化された入力イベントモデル
- `ActionManager` と `Action`: アクション登録・実行ハブ

### コンポジションとレイヤー基盤

- `Composition/*`: コンポジションバッファ、プリコンポーズ対応
- `Layer/*`: レイヤー状態、ストリップ、ブレンド/マット関連データ
- `Track/LayerTrack`: レイヤートラック抽象

### 画像とグラフィックス

- `Image/*`: 画像コンテナとフォーマット橋渡し
- `ImageProcessing/*`: OpenCV、DirectCompute、Halide パイプライン
- `Graphics/*`: GPU テクスチャ、シェーダー、PSO/キャッシュ、描画補助
- `Render/*`: レンダーキュー、設定、レンダーワーカー情報

### 動画とコーデック

- `Codec/*`: MF/FFmpeg/GStreamer のデコーダー/エンコーダー補助
- `Video/*`: デコーダー/エンコーダーIFと再生補助
- `Media/*`: メディアソース、プローブ、フレーム/音声デコーダーラッパー

### オーディオ

- `Audio/*`: ミキサー、レンダラー、パン、ダッキング、リングバッファ、波形
- 音声フォーマット/フレーム/プロバイダー抽象

### エフェクトと処理

- `Effect/*`: トランジションとエフェクト基盤ユーティリティ
- `Color/*` と `ColorCollection/*`: 色型、LUT、カラーグレーディング補助
- `Mask/RotoMask`: ロトスコープ/マスクデータモデル
- `Tracking/MotionTracker`: モーショントラッキング基盤

### ジオメトリ・変形・シェイプ

- `Geometry/*`: メッシュ入出力と数値計算補助
- `Mesh/*`: メッシュデータと Wavefront 対応
- `Transform/*`: 2D/3D 変形コンポーネント
- `Shape/*`: ベクターパス/レイヤー/グループ定義

### スクリプトと式

- `Script/*`: 式パーサー/評価器とスクリプトエンジン要素
- 組み込み関数と環境管理モジュール

### アセット・IO・プラットフォーム

- `Asset/*`: アセットメタデータ、プロキシ生成/キャッシュ
- `IO/*`: 非同期ファイル/画像の入出力ユーティリティ
- `File/*` と `FileSystem/*`: ファイル情報/種別とディレクトリ補助
- `Platform/*`: OS/プラットフォーム補助、プロセス/シェルユーティリティ

### ユーティリティと基盤

- `Utils/*`: ID、文字列、パス、タイマー、エクスプローラ補助
- `Container/*`: マルチインデックスコンテナ
- `Event/EventBus`: イベント配信基盤
- `Thread/*`: スレッド補助
- `EnvironmentVariable/*`: 環境変数参照/更新補助

## このリポジトリにある個別ドキュメント

- `docs/TimelineClock_Architecture.md`
- `docs/PlaybackClock_Usage.md`
- `docs/MFFrameExtractor_Usage.md`
- `docs/FrameRange_Usage.md`

## 補足

- このファイルは機能発見用の索引であり、完全なAPIリファレンスではありません。
- 実装時は上記の個別ドキュメントと対応する `include/*` モジュールを起点に確認してください。
