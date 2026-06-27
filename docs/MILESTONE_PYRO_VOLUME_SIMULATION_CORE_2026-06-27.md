# マイルストーン: Pyro Volume Simulation Core

> 2026-06-27 作成

## 目的

`ArtifactCore` に、煙・炎・蒸気・爆発の基礎となる voxel-based gas simulation を追加する。

Cinema 4D Pyro に近い制作体験を最終目標とするが、Core は UI や特定 renderer に依存せず、次の責務だけを持つ。

- emitter から volume field を生成する
- density / temperature / fuel / velocity を時間発展させる
- frame 単位で deterministic に評価・キャッシュする
- CPU reference と GPU backend が同じ入力契約を共有する
- renderer、particle、cache exporter が必要な field を明示的に取得できる

---

## 設計原則

1. 既存の particle simulation へ Pyro 状態を埋め込まず、独立した volume simulation とする
2. CPU reference solver を先に成立させ、GPU 実装の正解基準にする
3. `QImage` を simulation state、描画、転送に使用しない
4. CPU download / GPU upload / image 化は明示的な境界 API に限定する
5. `ArtifactCore` は scene object、Inspector、timeline UI を所有しない
6. renderer は field の所有者にならず、immutable な frame snapshot を受け取る
7. 新規の global signal / slot を追加せず、既存 service / snapshot / job 経路へ接続する
8. Diligent / D3D12 backend は CPU contract 完成後に最小範囲で追加する

---

## Core Responsibility Map

### `PyroDomain`

simulation bounds、voxel size、解像度、境界条件を保持する。

- world-space bounds
- voxel size / resolution
- fixed time step
- open / closed boundary
- memory budget

### `PyroFieldSet`

1 frame 分の volume state を所有する。

- scalar: density
- scalar: temperature
- scalar: fuel
- scalar: pressure
- scalar: divergence
- vector: velocity
- optional: color

MVP は dense grid で開始する。sparse tile / OpenVDB 互換は後段とし、最初から storage interface を solver へ直接露出させない。

### `PyroEmitter`

geometry または point source を field へ注入する。

- box / sphere primitive
- point / particle samples
- density / temperature / fuel / velocity injection
- surface / volume emission mode
- per-frame emission snapshot

### `PyroCollider`

solid occupancy と boundary velocity を生成する。

- box / sphere primitive
- static collider
- moving collider snapshot
- no-slip / free-slip boundary policy

### `PyroSolver`

field の時間発展を担当する。

- source injection
- semi-Lagrangian advection
- buoyancy
- vorticity confinement
- combustion
- cooling / dissipation
- divergence calculation
- pressure solve
- velocity projection

### `PyroSimulation`

solver、domain、emitter、collider、cache を束ねる。既存 `ISimulationSystem` との adapter はここに限定し、solver 自体を global manager に依存させない。

### `PyroFrameSnapshot`

renderer / cache / diagnostics に渡す immutable な結果契約。

- frame index / simulation time
- domain transform / resolution
- available field mask
- field views or backend resource handles
- simulation settings hash
- backend / precision metadata

---

## Phase 0: Contract and Memory Budget

### 作業項目

- field semantic と単位を定義する
- world / voxel 座標変換を定義する
- scalar / vector grid view の非所有 API を定義する
- frame input / output snapshot を定義する
- resolution ごとの概算メモリ量を diagnostics から取得可能にする
- CPU / GPU capability と fallback reason を typed にする

### 完了条件

- renderer や UI を import せず Core API が自己完結する
- field の暗黙コピー、暗黙 readback が存在しない
- 128x128x128 domain の field 別メモリ量を事前計算できる

---

## Phase 1: CPU Dense Grid Foundation

### 作業項目

- contiguous scalar grid
- contiguous vector grid
- trilinear sampling
- gradient / divergence / curl helper
- border sampling policy
- double-buffered field swap
- grid clear / resize / snapshot

### 完了条件

- scalar と vector field を安全に生成・sample できる
- out-of-bounds policy が全 helper で一致する
- simulation hot path に Qt image 型が入らない

---

## Phase 2: Smoke Reference Solver

### 作業項目

- fixed-step integration
- primitive emitter injection
- density / temperature / velocity advection
- buoyancy
- dissipation / cooling
- pressure projection
- reset / seek-from-start

### 完了条件

- box / sphere emitter から煙が上昇する
- projection 後の divergence が設定した許容値以下になる
- 同じ初期 state、seed、設定、frame から同じ CPU 結果を得る
- negative delta や巨大 delta を直接 solver に流さない

---

## Phase 3: Fire and Art Direction

### 作業項目

- fuel injection
- ignition threshold
- burn rate
- heat / smoke generation
- flame cooling
- vorticity confinement
- wind / directional force
- optional color field
- quality preset を solver parameter set として定義

### 完了条件

- smoke-only / fire / steam の preset 相当を同じ solver で表現できる
- temperature と fuel の非物理値を clamp できる
- art-direction parameter が solver 内の magic number にならない

---

## Phase 4: Collider and Source Integration

### 作業項目

- primitive collider voxelization
- moving collider velocity
- particle sample emitter adapter
- mesh voxelization は Core geometry snapshot から受け取る
- emitter / collider の frame snapshot hash

### 完了条件

- 煙が primitive collider を貫通しない
- 既存 particle output を point emitter として利用できる
- Core が Artifact layer や scene object を直接参照しない

---

## Phase 5: Deterministic Frame Cache

### 作業項目

- cache key: domain + settings + sources + colliders + frame dependency
- memory cache
- disk cache format versioning
- field 単位の保存選択
- cache invalidation reason
- nearest checkpoint からの resume
- cancellation-safe write

### 完了条件

- timeline seek が必ず frame 0 からの全再計算を要求しない
- density-only cache と fire cache を区別できる
- 破損・旧version cache を安全に拒否できる
- partial file を valid cache として公開しない

### 非目標

- この段階での OpenVDB 読み書き
- hardware 間の bit-exact GPU 再現

---

## Phase 6: GPU Compute Backend

### 作業項目

- Core contract に沿った GPU field storage
- injection / advection / force / divergence / pressure / projection pass
- ping-pong texture management
- explicit upload / readback API
- CPU / GPU backend selection
- device loss / unsupported capability fallback
- pass timing と memory diagnostics

### 完了条件

- CPU と GPU が同じ scene input snapshot を受け取れる
- readback なしで renderer に field resource を渡せる
- CPU reference と許容誤差内で主要 field を比較できる
- backend 不成立時に理由付きで CPU fallback できる

### 注意

- Diligent backend や `libs/DiligentEngine` の変更を前提にしない
- D3D12 固有処理を public solver contract へ漏らさない
- shader / resource lifetime の設計確認前に低レベル実装を広げない

---

## Phase 7: Renderer and Artifact Integration Contract

この Phase では Core 側に renderer を実装せず、必要な受け渡し契約を完成させる。

### Core 側

- density / temperature / color / velocity field descriptor
- world-to-volume transform
- CPU view または GPU resource view
- blackbody / transfer-function parameter data
- motion vector 用 velocity access

### Artifact 側の別マイルストーン

- Pyro layer / object
- emitter / collider binding
- Inspector property editor
- timeline bake / clear cache
- viewport raymarch renderer
- software render fallback
- project serialization

### 完了条件

- Core snapshot だけから別 renderer が volume を描画できる
- renderer が mutable simulation state を直接参照しない
- UI 未接続でも headless frame simulation が成立する

---

## Phase 8: Sparse Volume and Interchange

### 作業項目

- sparse tiled field storage
- active tile bounds
- adaptive domain growth の検討
- OpenVDB / NanoVDB adapter の技術評価
- density / temperature / velocity cache interchange
- upres の入力契約

### 完了条件

- dense solver contract を壊さず storage backend を交換できる
- 未使用領域の大きい domain で memory 使用量を削減できる
- 外部 format 変換が simulation hot path から分離される

---

## 推奨モジュール境界

名称は実装開始時に module graph を再確認して確定する。

- `Simulation.Pyro.Types`
- `Simulation.Pyro.Grid`
- `Simulation.Pyro.Source`
- `Simulation.Pyro.Solver`
- `Simulation.Pyro.Cache`
- `Simulation.Pyro.Backend`

`.ixx` には value type、view、abstract contract の最小宣言だけを置き、solver step、storage、backend dependency は `.cppm` に閉じる。既存 module の再 export は行わない。

---

## MVP Definition

最初の usable milestone は Phase 0〜5 とする。

- dense CPU grid
- box / sphere emitter
- smoke / fire
- primitive static collider
- deterministic fixed-step simulation
- checkpoint cache
- `PyroFrameSnapshot`

GPU real-time preview、mesh collider、OpenVDB、adaptive sparse domain は MVP に含めない。

---

## Diagnostics

- domain resolution / voxel count
- field memory usage
- active voxel bounds
- substep count
- pressure iteration count / residual
- frame simulation time
- cache hit / miss / invalidation reason
- backend / fallback reason

diagnostics は読み取り API とし、新しい global event wiring を要求しない。

---

## リスク

### メモリ

複数の float field と ping-pong buffer により、解像度の3乗で増加する。voxel size の下限、memory budget、最大解像度 guard を Phase 0 で入れる。

### Timeline Seek

Pyro は前 frame 依存であり任意 frame を直接評価できない。checkpoint cache と invalidation hash を Core 契約の一部にする。

### CPU / GPU 差

bit-exact を要求せず、field ごとの許容誤差と視覚的基準を定義する。CPU reference を消さない。

### Module Dependency

graphics backend を solver interface へ import すると循環しやすい。GPU resource は backend view / opaque handle 境界に閉じる。

---

## Non-Goals

- Cinema 4D Pyro との完全互換
- 初期段階での physically exact combustion
- Core 内の Qt UI
- Core 内の volume material editor
- renderer による simulation state 所有
- 暗黙の CPU / GPU 転送
- 既存 particle system の置換
- `libs/DiligentEngine` fork の変更

---

## Related

- `ArtifactCore/include/Core/SimulationSystem.ixx`
- `ArtifactCore/include/Math/SpatialGrid.ixx`
- `ArtifactCore/include/Graphics/ParticleCompute.ixx`
- `ArtifactCore/include/Graphics/GPUComputeContext.ixx`
- `ArtifactCore/docs/MILESTONE_DETERMINISTIC_SNAPSHOT_CAPABILITY_CONTRACT_2026-04-09.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md`

## Current Status

2026-06-27 時点では設計のみ。実装、CMake 登録、build、test は未実施。
