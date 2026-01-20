# Timeline Clock アーキテクチャ

## 設計哲学

### 問題: Signal/Slot の高精度タイミング問題

Qt の Signal/Slot は便利ですが、**高精度タイミングには適していません**：

1. **イベントキューイング遅延**: Signal は Qt のイベントループでキューイングされるため、数ミリ秒の遅延が発生
2. **スレッド間通信**: 異なるスレッド間では `QueuedConnection` により、さらに遅延が増加
3. **UI更新は60fpsで十分**: タイムコード表示やシークバーの更新には、マイクロ秒精度は不要

### 解決策: 2層アーキテクチャ

```
┌─────────────────────────────────────────────┐
│ ArtifactCore::TimelineClock                 │
│ - マイクロ秒精度のタイミング                │
│ - Qt非依存                                  │
│ - スレッドセーフ                            │
└─────────────────────────────────────────────┘
           ↓ 直接参照（高精度）
┌─────────────────────────────────────────────┐
│ Composition / Layer                         │
│ - オーディオ同期                            │
│ - ビデオレンダリング                        │
│ - フレーム精度の計算                        │
└─────────────────────────────────────────────┘

           ↓ ポーリング（60fps = 16ms間隔）
┌─────────────────────────────────────────────┐
│ Artifact::ArtifactTimelineClock (QObject)   │
│ - QTimer で16ms間隔で状態をチェック         │
│ - Signal/Slot でUI層に通知                  │
└─────────────────────────────────────────────┘
           ↓ Signal/Slot
┌─────────────────────────────────────────────┐
│ UI Widgets                                  │
│ - タイムコード表示                          │
│ - シークバー                                │
│ - 再生/停止ボタン                           │
└─────────────────────────────────────────────┘
```

## 使い方

### 1. Composition での使用（高精度タイミング）

```cpp
// Composition クラス
class ArtifactAbstractComposition {
private:
    ArtifactTimelineClock* uiClock_;  // UI通知用
    
public:
    void setTimelineClock(ArtifactTimelineClock* clock) {
        uiClock_ = clock;
    }
    
    void renderFrame() {
        // 高精度クロックに直接アクセス
        auto* highPrecisionClock = uiClock_->clock();
        
        // マイクロ秒精度でフレーム位置を取得
        auto currentFrame = highPrecisionClock->currentFrame();
        auto deltaTime = highPrecisionClock->deltaTime();
        
        // オーディオ同期
        if (audioEngine_) {
            auto audioTime = audioEngine_->currentTime();
            highPrecisionClock->syncToAudioClock(audioTime);
        }
        
        // レンダリング処理
        for (auto& layer : layers_) {
            layer->render(currentFrame, deltaTime);
        }
    }
};
```

### 2. UI Widget での使用（Signal/Slot）

```cpp
// タイムラインウィジェット
class TimelineWidget : public QWidget {
    Q_OBJECT
    
private:
    ArtifactTimelineClock* clock_;
    QLabel* timecodeLabel_;
    
public:
    TimelineWidget(QWidget* parent = nullptr) 
        : QWidget(parent)
        , clock_(new ArtifactTimelineClock(this))
    {
        // UI更新用のSignal/Slotを接続
        connect(clock_, &ArtifactTimelineClock::timecodeChanged,
                this, [this](const QString& timecode) {
            timecodeLabel_->setText(timecode);
        });
        
        connect(clock_, &ArtifactTimelineClock::playbackStateChanged,
                this, &TimelineWidget::onPlaybackStateChanged);
        
        connect(clock_, &ArtifactTimelineClock::tickFrame,
                this, &TimelineWidget::updateSeekBar);
    }
    
private slots:
    void onPlayButtonClicked() {
        clock_->start();
    }
    
    void onPlaybackStateChanged(bool isPlaying) {
        playButton_->setText(isPlaying ? "Pause" : "Play");
    }
    
    void updateSeekBar(const FramePosition& position) {
        seekBar_->setValue(position.frame());
    }
};
```

### 3. オーディオエンジンでの使用（高精度）

```cpp
class AudioEngine {
private:
    TimelineClock* clock_;  // 高精度クロックを直接参照
    
public:
    void processAudio(float* buffer, int frameCount) {
        // マイクロ秒精度でオーディオ同期
        auto currentTime = clock_->elapsedTime();
        auto currentFrame = clock_->currentFrame();
        
        // オーディオサンプルを生成
        for (int i = 0; i < frameCount; ++i) {
            buffer[i] = generateSample(currentFrame, i);
        }
        
        // タイムライン側に同期情報をフィードバック
        auto audioTime = std::chrono::microseconds(/* audio clock time */);
        clock_->syncToAudioClock(audioTime);
    }
};
```

## パフォーマンス比較

### Signal/Slot経由（推奨しない）
```cpp
// ? 遅延が大きい
connect(clock, &Clock::frameChanged, 
        layer, &Layer::updateFrame);
// 遅延: 5?10ms（イベントキューイング）
```

### 直接参照（推奨）
```cpp
// ? 遅延なし
auto frame = clock->clock()->currentFrame();
layer->updateFrame(frame);
// 遅延: < 1μs（関数呼び出しのみ）
```

## ベストプラクティス

### ? 推奨される使い方

1. **Composition/Layer**: `clock()` で高精度クロックを直接参照
2. **UI Widget**: Signal/Slot で通知を受け取る（16ms間隔で十分）
3. **オーディオエンジン**: 高精度クロックを直接参照
4. **ビデオエンコーダー**: 高精度クロックを直接参照

### ? 避けるべき使い方

1. **オーディオ処理に Signal/Slot を使用**: 遅延により音ズレが発生
2. **フレーム精度の計算に Signal/Slot を使用**: タイミングのずれが累積
3. **60fps以上の頻度で Signal を発行**: Qt のイベントループが飽和

## FAQ

### Q: なぜ2つのクラスに分けたのか？

A: 関心の分離（Separation of Concerns）：
- **TimelineClock**: 高精度タイミングロジック（コアドメイン）
- **ArtifactTimelineClock**: UI通知レイヤー（プレゼンテーション層）

### Q: Signal/Slot を完全に避けるべきか？

A: いいえ。UI更新には適しています：
- タイムコード表示: 16ms間隔で十分
- シークバー: 16ms間隔で十分
- 再生/停止ボタン: ユーザー操作なので遅延は問題なし

### Q: マルチスレッドでの使用は安全か？

A: はい。`TimelineClock` は内部で `QMutex` を使用しているため、スレッドセーフです。

### Q: オーディオとビデオの同期はどうするか？

A: `syncToAudioClock()` メソッドを定期的に呼び出して、オーディオクロックに同期します：

```cpp
void Composition::update() {
    auto* highPrecisionClock = clock_->clock();
    
    // オーディオエンジンから現在時刻を取得
    auto audioTime = audioEngine_->currentTime();
    
    // タイムラインクロックと同期
    highPrecisionClock->syncToAudioClock(audioTime);
    
    // ずれをチェック
    auto offset = highPrecisionClock->audioOffset();
    if (std::abs(offset.count()) > 1000) {  // 1ms以上ずれ
        qWarning() << "Audio/Video sync drift:" << offset.count() << "μs";
    }
}
```

## まとめ

この2層アーキテクチャにより：
- ? 高精度タイミングが必要な部分は直接参照
- ? UI更新は Signal/Slot で簡潔に記述
- ? オーディオとビデオの同期が正確
- ? コードの責任が明確に分離

**重要**: Signal/Slot は便利ですが、高精度タイミングには適していません。用途に応じて使い分けてください。
