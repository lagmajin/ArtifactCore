# オーディオ再生パフォーマンス設計

> 最終更新: 2026-06-05
> 関連ファイル: `AudioRenderer.cppm`, `AudioRingBuffer.cppm`, `AudioBus.cppm`, `AudioCache.cppm`, `WASAPIBackend.cppm`, `ArtifactPlaybackEngine.cppm`

---

## 1. スレッドモデル

```
┌─────────────────────────────────┐
│  PlaybackEngine worker thread   │  QThread::TimeCriticalPriority
│  (Producer)                     │  composition→getAudio() → ringBuffer.write()
├─────────────────────────────────┤
│  WASAPI render thread           │  std::thread, WaitForMultipleObjects
│  (Consumer)                     │  ringBuffer.read() → audioCallback() → ハードウェア
├─────────────────────────────────┤
│  Main / UI thread               │  level metering (QMetaObject::invokeMethod queued)
└─────────────────────────────────┘
```

**黄金規則: リアルタイムコールバック (`audioCallback`) 内では以下の操作を絶対にしないこと。**

| 禁止操作 | 理由 |
|----------|------|
| `qDebug()` / `qWarning()` | 文字列フォーマット + ロック取得が伴う |
| `std::function` のコピー | heap アロケーションが発生する可能性 |
| `QVector::resize()` (新規) | heap アロケーションが発生する |
| `std::log10()` / `std::sqrt()` の乱用 | 関数呼び出しコスト（まとめ処理で十分） |
| `std::mutex::lock()` | デッドロックリスク + スケジューリング遅延 |
| `new` / `delete` | メモリアロケーターが RT 非対応 |

---

## 2. リングバッファ設計 (SPSC Lock-free)

**ファイル**: `AudioRingBuffer.cppm`

- **容量**: 48000 × 8 = 384,000 フレーム（8 秒 @48kHz）
- **Producer**: PlaybackEngine スレッド (`write`, `clear`)
- **Consumer**: WASAPI スレッド (`read`)
- `writeCount_` / `readCount_` は **cache-line aligned (64-byte)** で false sharing を防止
- `clearGeneration_` で producer → consumer へのバッファ破棄通知を lock-free で行う

```
[Producer] writeCount_++ ──→ [Consumer] read() で読み取り
                    clearGeneration_++ ──→ [Consumer] read() で検出し readCount_ をスキップ
```

**注意**: `read()` 内で `data.channelData.resize()` が呼ばれる。呼び出し側で事前に同じサイズを確保していれば no-op になる。

---

## 3. audioCallback の設計

**ファイル**: `AudioRenderer.cppm`

```cpp
void audioCallback(float *buffer, int frames, int channelsRequested)
```

### 処理フロー
1. `active` / `isMute` / `masterVolumeLinear` を atomic で読み取り
2. `readBuffer_` (再利用可能バッファ) を zero-fill して ring buffer から read
3. 出力バッファに volume 適用 + clamp + partial underflow 時の cosine fade
4. RMS / peak レベルを計算（4 回に 1 回スロットリング）
5. `levelCallback` を shared_ptr + atomic_load で lock-free 呼び出し

### 部分 Underflow 対処
- availableFrames < frames の場合、`availableFrames - 64` 以降に **cosine fade** を適用
- 64 サンプルのフェードアウトでクリックノイズを防止

### Level Metering
- `levelCallback` は `std::atomic_load` で shared_ptr を読み取り、mutex ロックなし
- `levelCallbackCounter` で **4 回に 1 回** にスロットリング（RMS/peak 計算のコスト削減）

---

## 4. PlaybackEngine のオーディオ更新

**ファイル**: `ArtifactPlaybackEngine.cppm`

### updateAudio() の処理フロー
1. composition に音声があるか確認
2. デバイスが未開放なら `openDevice()` を試行
3. **dirty flag**: volume/mute の変化時のみ `setMasterVolume()` / `setMute()` を呼ぶ
   - `std::pow(10, dB/20)` は毎ループ不要 → sentinel + 変化検知で回避
4. `audioSampleAccumulator_` で小数部を蓄積し、整数誤差によるドリフトを防止
5. `composition_->getAudio()` でデコード → `audioRenderer_->enqueue()` で ring buffer に書き込み
6. バッファ残量に応じた **adaptive sleep**:
   - バッファ満杯 → surplus に応じて最大 2ms sleep
   - バッファ半分以上 → `yield()`
   - バッファ半分以下 → **500us sleep** (完全な tight spin を防止)

### Audio/Video Sync
- `syncWithAudioClock()` で audio clock と比較
- 33ms (1 フレーム@30fps) 以上ずれた場合にベース時間を書き換え
- EMA フィルタ (`AudioClockProvider`) と連携して滑らかに追従

---

## 5. WASAPI Backend の設計

**ファイル**: `WASAPIBackend.cppm`

- **Shared mode**, **50ms バッファ** (2400 frames @48kHz)
- `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` でイベント駆動
- レンダースレッドは `WaitForMultipleObjects` で audio event + stop event を待機
- Float32 / Int16 両方に対応（Int16 の場合は一時バッファで変換）

### バッファサイズのationale
- 50ms は shared mode での妥協値。低レイテンシを実現するには exclusive mode が必要
- OS スケジューラの遅延に対するヘッドマージンとして確保

---

## 6. AudioCache の LRU

**ファイル**: `AudioCache.cppm`

- **LRU 方式**: `lastAccess` (ms) ベース
- **クリーンアップ**: `std::nth_element` で中央値を O(n) で求め、古い半分を削除
  - `std::sort` は O(n log n) のため `nth_element` を使用
- キャッシュサイズ超過時に発動
- **prefetch** はスタブ（外部から呼び出される）

---

## 7. AudioBus の処理パイプライン

**ファイル**: `AudioBus.cppm`

```
[Input] → addInput() (downmix if needed) → FX Rack (Pre-fader) → Volume/Pan (Post-fader) → Metering → [Output]
```

- `process()` 内の volume/metering ループは **scalar**（`// TODO: SIMD optimization`）
- `addInput()` で downmix 必要な場合のみ `AudioDownMixer` を経由
- `addSideChain()` は side chain buffer に加算

---

## 8. 既知の制約と将来の改善候補

| 項目 | 現状 | 改善候補 |
|------|------|----------|
| audioCallback 内の log | qWarning を削除済み | プロファイラカウンタで監視 |
| readBuffer_ の resize | 再利用で回避済み | channelCount 固定ならさらに最適化可 |
| AudioBus volume loop | scalar | SIMD (SSE/AVX) 化 |
| WASAPI バッファサイズ | 50ms 固定 | 設定可能にする (5-50ms) |
| AudioCache LRU | nth_element で改善済み | ring buffer 方式への移行 |
| Level metering | 4 回に 1 回 | 可変スロットリング |

---

## 9. パフォーマンス監視

**ファイル**: `PerformanceProfiler.ixx`

`AudioEngineProfiler` (singleton, lock-free) が以下を記録:

- **callback duration**: WASAPI コールバックの実行時間 (ns)
- **fill loop duration**: `updateAudio()` の実行時間 (ns)
- **buffer level**: ring buffer の充填率 (%)
- **underflow count**: バッファ枯渇回数

UI には `ProfilerPanelWidget` で表示。

---

## 10. 変更履歴

| 日付 | 変更 | 理由 |
|------|------|------|
| 2026-06-05 | audioCallback 内 qWarning 削除 | RT スレッド上のコスト削減 |
| 2026-06-05 | levelCallback を shared_ptr+atomic 化 | mutex ロック + std::function コピー排除 |
| 2026-06-05 | setMasterVolume/setMute を dirty flag 方式に | 毎ループの std::pow 計算を排除 |
| 2026-06-05 | readBuffer_ 再利用で heap アロケ排除 | コールバック毎の QVector アロケーション排除 |
| 2026-06-05 | fill loop tight spin に 500us sleep 追加 | UI スレッドの応答性維持 |
| 2026-06-05 | AudioCache LRU を nth_element に変更 | O(n log n) → O(n) 改善 |
| 2026-06-05 | 頻繁な qDebug/qWarning をスロットリング | ログ出力コスト削減 |
