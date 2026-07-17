# Render Foundation Triad

**ステータス:** In Progress

## 目的

既存の Composition / SceneNode と Diligent backend を直接結合せず、描画差分、パス依存、時間履歴を独立して扱える基盤を用意する。

## Phase 1: Render Index Foundation

- 平坦な `RenderProxyDescriptor` と安定 ID
- transform / geometry / material / visibility / instances の dirty mask
- 全体または dirty-only snapshot
- Cloner を prototype + instance range として表現できる契約

## Phase 2: Render Graph Foundation

- texture / buffer resource descriptor
- graphics / compute / copy pass descriptor
- read/write 依存からの実行順コンパイル
- transient resource の read-before-write と循環検出
- transient allocationへ接続可能な lifetime range

## Phase 3: Temporal History Foundation

- composition / view / pass / resolution / quality を含む履歴キー
- opaque GPU resource token の登録
- camera cut / time discontinuity / scene change 等の明示的な無効化
- 連続フレームだけを再利用する判定

## 現在の範囲

契約と CPU 側管理のみ。Composition、GI、Pointwise Fusion、Diligent resource/barrier、実描画パスへの接続は未着手。

## 次段階

1. Composition evaluator から dirty-only Render Index snapshot を生成する adapter
2. GI execution plan と Pointwise Fusion を Render Graph pass へ変換する adapter
3. Diligent texture/buffer ownership と Temporal History resource token の接続
4. diagnostics に pass order / lifetime / invalidation reason を公開
