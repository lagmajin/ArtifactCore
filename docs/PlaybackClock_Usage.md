# PlaybackClock 使用ガイド

## 概要

`PlaybackClock` は、CompositionとLayerの再生タイミングを管理する高精度クロックです。

## 主な機能

### 1. 基本的な再生制御

```cpp
import Playback.Clock;
import Frame.Rate;

// クロックの作成
PlaybackClock clock(FrameRate(30.0));  // 30fps

// 再生開始
clock.start();

// 現在のフレーム取得
int64_t currentFrame = clock.currentFrame();

// タイムコード取得
QString timecode = clock.timecode();  // "00:00:05:15"

// 一時停止
clock.pause();

// 再開
clock.resume();

// 停止
clock.stop();
```

### 2. 再生速度制御

```cpp
// 通常再生
clock.setPlaybackSpeed(1.0);

// 半速再生（スローモーション）
clock.setPlaybackSpeed(0.5);

// 倍速再生
clock.setPlaybackSpeed(2.0);

// 逆再生
clock.setPlaybackSpeed(-1.0);

// 逆再生（半速）
clock.setPlaybackSpeed(-0.5);
```

### 3. ループ再生

```cpp
// ループ範囲を設定（フレーム100?300）
clock.setLoopRange(100, 300);

// ループ解除
clock.clearLoopRange();

// ループ状態確認
if (clock.isLooping()) {
    int64_t start = clock.loopStartFrame();
    int64_t end = clock.loopEndFrame();
}
```

### 4. オーディオ同期

```cpp
// オーディオ同期を有効化
clock.setAudioSyncEnabled(true);

// オーディオクロックと同期
std::chrono::microseconds audioTime = getAudioClockTime();
clock.syncToAudioClock(audioTime);

// 同期のずれを取得
auto offset = clock.audioOffset();
if (std::abs(offset.count()) > 1000) {
    // 1ms以上ずれている
}
```

### 5. フレームシーク

```cpp
// フレーム番号でシーク
clock.setFrame(150);

// FramePosition でシーク
FramePosition pos(250);
clock.setPosition(pos);

// 現在位置取得
FramePosition current = clock.currentPosition();
```

### 6. ドロップフレーム検出

```cpp
// ドロップフレーム検出を有効化
clock.setDropFrameDetectionEnabled(true);

// 再生中...

// ドロップしたフレーム数を取得
int64_t dropped = clock.droppedFrameCount();
if (dropped > 0) {
    qWarning() << "Dropped frames:" << dropped;
}

// カウントをリセット
clock.resetDroppedFrameCount();
```

### 7. デルタタイム（フレーム間の経過時間）

```cpp
// 前回の更新からの経過時間を取得
auto delta = clock.deltaTime();

// レンダリングループで使用
while (clock.isPlaying()) {
    auto dt = clock.deltaTime();
    
    // レンダリング処理
    render(clock.currentFrame(), dt);
    
    // 60fps で更新
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

## Composition での使用例

```cpp
class ArtifactAbstractComposition {
private:
    PlaybackClock playbackClock_;
    
public:
    void play() {
        playbackClock_.start();
        isPlaying_ = true;
        
        // 全レイヤーに再生開始を通知
        for (auto& layer : layers_) {
            layer->onPlaybackStart(playbackClock_.currentPosition());
        }
    }
    
    void pause() {
        playbackClock_.pause();
        isPlaying_ = false;
        
        // 全レイヤーに一時停止を通知
        for (auto& layer : layers_) {
            layer->onPlaybackPause(playbackClock_.currentPosition());
        }
    }
    
    void update() {
        if (!isPlaying_) return;
        
        // 現在のフレーム位置を取得
        FramePosition currentPos = playbackClock_.currentPosition();
        
        // デルタタイムを取得
        auto deltaTime = playbackClock_.deltaTime();
        
        // 全レイヤーを更新
        for (auto& layer : layers_) {
            layer->update(currentPos, deltaTime);
        }
        
        // オーディオ同期
        if (audioEngine_) {
            auto audioTime = audioEngine_->currentTime();
            playbackClock_.syncToAudioClock(audioTime);
        }
    }
    
    void setPlaybackSpeed(double speed) {
        playbackClock_.setPlaybackSpeed(speed);
        
        // オーディオエンジンにも速度変更を通知
        if (audioEngine_) {
            audioEngine_->setSpeed(speed);
        }
    }
};
```

## Layer での使用例

```cpp
class ArtifactAbstractLayer {
public:
    virtual void update(const FramePosition& position, 
                       std::chrono::microseconds deltaTime) {
        // フレーム位置に基づいてレイヤーの状態を更新
        updateTransform(position);
        updateEffects(position);
        
        // デルタタイムを使ったアニメーション
        if (isAnimating_) {
            double seconds = deltaTime.count() / 1000000.0;
            animationProgress_ += seconds * animationSpeed_;
        }
    }
    
    virtual void onPlaybackStart(const FramePosition& startPos) {
        // 再生開始時の初期化
        cacheFrame(startPos);
    }
    
    virtual void onPlaybackPause(const FramePosition& pausePos) {
        // 一時停止時の処理
        flushCache();
    }
};
```

## マルチスレッド対応

PlaybackClockはスレッドセーフです：

```cpp
// メインスレッドで再生制御
clock.start();

// レンダリングスレッド
std::thread renderThread([&clock]() {
    while (clock.isPlaying()) {
        auto frame = clock.currentFrame();
        renderFrame(frame);
    }
});

// オーディオスレッド
std::thread audioThread([&clock]() {
    while (clock.isPlaying()) {
        auto time = clock.elapsedTime();
        processAudio(time);
    }
});
```

## パフォーマンス最適化

### 1. フレームキャッシング

```cpp
class LayerCache {
    std::unordered_map<int64_t, CachedFrame> cache_;
    PlaybackClock* clock_;
    
public:
    void update() {
        int64_t currentFrame = clock_->currentFrame();
        double speed = clock_->playbackSpeed();
        
        // 再生方向を考慮してプリキャッシュ
        if (speed > 0) {
            // 順再生：先読み
            for (int i = 1; i <= 5; ++i) {
                precache(currentFrame + i);
            }
        } else if (speed < 0) {
            // 逆再生：後ろ読み
            for (int i = 1; i <= 5; ++i) {
                precache(currentFrame - i);
            }
        }
    }
};
```

### 2. 適応的品質制御

```cpp
void adaptiveRender() {
    auto droppedFrames = clock.droppedFrameCount();
    
    if (droppedFrames > 10) {
        // ドロップフレームが多い場合、品質を下げる
        renderQuality = RenderQuality::Draft;
    } else {
        renderQuality = RenderQuality::Full;
    }
    
    clock.resetDroppedFrameCount();
}
```

## デバッグ

```cpp
// 詳細な統計情報を表示
QString stats = clock.statistics();
qDebug() << stats;

// タイムコード表示
QString tc = clock.timecodeWithSubframe();
statusBar->setText(tc);  // "00:00:05:15.42"
```

## 注意事項

1. **フレームレート変更**: 再生中にフレームレートを変更すると、タイミングが再計算されます
2. **オーディオ同期**: 音ズレを防ぐため、定期的に `syncToAudioClock()` を呼び出してください
3. **スレッドセーフ**: すべてのメソッドはスレッドセーフですが、パフォーマンスのため頻繁な呼び出しは避けてください
4. **ループ再生**: ループ範囲外にシークすると、ループがリセットされます
