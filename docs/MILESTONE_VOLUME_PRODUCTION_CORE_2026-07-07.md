# マイルストーン: Volume Production Core

> 2026-07-07 作成

## 目的

`ArtifactCore` に、プロシージャルなボリューム制作に必要なコア基盤を追加する。
AE の Turbulent Noise / Fractal のような映像制作用プロシージャルボリュームを可能にする。

## 背景

- `Core.Simulation.Pyro` — 物理煙シミュレーション完備
- `Render.VolumeRenderer` — CPU emission-absorption raymarcher 完備
- `Render.VolumeRenderer.VolumetricLight` — AE風スポット/ポイントライト完備
- しかしプロシージャルノイズ、メッシュ→ボリューム変換、フィールド修飾子が存在しない

## Phase 1: Noise / Procedural Field Generator

### 作業項目

- 2D/3D Perlin Noise (classic gradient noise)
- 2D/3D Simplex Noise
- 2D/3D Worley/Voronoi Noise (Cellular Noise)
- Fractal Brownian Motion (fbm) — octave + lacunarity + gain
- Curl Noise — 非圧縮ベクトル場
- Turbulence — 絶対値ノイズ
- Domain Warping (2-iteration)
- Seed / deterministic な再現性
- `VolumeScalarField` / `VolumeVectorField` への直接書き込み

### 完了条件

- 任意の解像度の dense volume field を純粋なプロシージャルノイズで初期化できる
- 異なる seed で異なるが再現可能な結果を得る
- curl noise で Pyro の初期速度場を生成できる
- renderer や UI に依存しない

### モジュール名

`Render.NoiseField`

---

## Phase 2: Mesh-to-Volume / SDF Converter

### 作業項目

- 三角形メッシュ→SDF voxelization
- 表面/内部判定 (winding number または ray stabbing)
- 距離フィールド生成
- `VolumeScalarField` 出力
- ボリューム表面の密度フォールオフ制御
- 既存 `Triangle.ixx`、`Mesh.ixx` の再利用

### 完了条件

- 任意の close mesh から density volume を生成できる
- テキストジオメトリ、3D文字、キャラクターを煙/炎として扱える
- renderer や UI に依存しない

### モジュール名

`Render.MeshToVolume`

---

## Phase 3: Volume Field Modifier Stack

### 作業項目

- Turbulence Noise Displace
- Wind Advection (custom vector field)
- Vortex / Swirl modifier
- Stretch / Scale modifier
- Smooth / Blur modifier
- Clamp / Threshold
- Multiply / Add blend
- Field remap (curve/gradient)
- Modifier stack (chain)

### 完了条件

- Noise + Modifier だけで静止したプロシージャル雲を生成できる
- 複数の modifier を pipe で接続できる
- 各 modifier が immutable input / new output パターンを持つ

### モジュール名

`Render.VolumeModifier`

---

## Non-Goals

- OpenVDB 依存（Phase 7 sparse volume で再評価）
- Real-time GPU preview（Phase 6 GPU backend 待ち）
- 物理精度（厳密な放射輸送は不要）
- Qt / Diligent / UI 依存

## Related

- `ArtifactCore/include/Render/VolumeRenderer.ixx`
- `ArtifactCore/include/Simulation/PyroSimulation.ixx`
- `ArtifactCore/docs/MILESTONE_PYRO_VOLUME_SIMULATION_CORE_2026-06-27.md`