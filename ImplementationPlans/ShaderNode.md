# Shader Node System (Project Roadmap & Progress)

## 🎯 プロジェクトの目的
Blenderの「Shader Node」やUnreal Engineの「Material Editor」に相当する、有向非巡回グラフ(DAG)ベースのマテリアル／シェーダー構築システムのコア基盤を作成する。
GUIエディタ上でユーザーが繋いだノード群から、リアルタイムで1つの巨大なHLSL/GLSLコード文字列を自動生成（コードジェネレート）し、JITコンパイルして描画に反映させるためのバックエンドアーキテクチャ。

## 📅 実装フェーズと進捗

### [x] フェーズ 1: コアデータ構造とコードジェネレータの作成 (完了)
*   UIを持たない純粋なC++のグラフデータ構造の実装。
*   `Node`, `Pin`, `Link`, `NodeGraph` などのクラス定義。
*   グラフから依存関係を解析し、HLSL文字列を組み立てるジェネレータのプロトタイプ作成。
*   **配置場所**: `ArtifactCore\include\ShaderNode`, `ArtifactCore\src\ShaderNode`
*   **モジュール名**: `Artifact.ShaderNode.Core`

### [ ] フェーズ 2: 実行時コンパイル(JIT)との結合 (未着手)
*   生成されたHLSL文字列を `Artifact::Graphics::Shader` などの実行時コンパイラ（D3DCompile / DXC）に渡し、動的にコンパイルできるかテストする。
*   コンパイルされたシェーダーをダミーの `Plane Layer` やメッシュに割り当てて描画を検証する。

### [ ] フェーズ 3: ノードエディタ(UI)の作成 (未着手)
*   Qt (QGraphicsView / NodeEditorライブラリ) をベースとしたUI構築。
*   UI操作（ノード追加、結線）に応じて C++の `NodeGraph` を構築し、バックグラウンドでコード生成〜コンパイルを走らせる連携処理。

### [ ] フェーズ 4: 実用的なシェーダーノードの拡充 (未着手)
*   `TextureNode`, `MathNode`, `ColorNode`, `MixRGBNode`, `NoiseNode` (Perlin/Fractal等) など、具体的なノード実装を増やす。
*   BSDFやライティングと結合するためのPBR対応出力ノードの設計。

---
*※このファイルはAIと開発者がプロジェクトの文脈とゴールを思い出すためのメモです。次のステップに着手する際はフェーズ2から進めてください。*
