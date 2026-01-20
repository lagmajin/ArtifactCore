# FrameRange 使用ガイド

## 概要

`FrameRange` は、動画編集におけるタイムライン上の範囲を表すクラスです。
ワークエリア、レイヤーの持続時間、再生範囲などを表現します。

## 基本的な使い方

### 1. 範囲の作成

```cpp
import Frame.Range;
import Frame.Position;

// フレーム番号で作成
FrameRange range1(100, 200);  // フレーム100?200

// FramePosition で作成
FramePosition start(100);
FramePosition end(200);
FrameRange range2(start, end);

// 長さを指定して作成
FrameRange range3 = FrameRange::fromDuration(100, 50);  // 100から50フレーム

// 特殊な範囲
FrameRange invalid = FrameRange::invalid();   // 無効な範囲
FrameRange infinite = FrameRange::infinite(); // 無限範囲
FrameRange zero = FrameRange::zero();         // 長さ0
```

### 2. 範囲情報の取得

```cpp
FrameRange range(100, 200);

qDebug() << "Start:" << range.start();           // 100
qDebug() << "End:" << range.end();               // 200
qDebug() << "Duration:" << range.duration();     // 100
qDebug() << "Length:" << range.length();         // 100 (durationと同じ)

// FramePosition として取得
FramePosition startPos = range.startPosition();
FramePosition endPos = range.endPosition();

// 検証
qDebug() << "Valid:" << range.isValid();      // true
qDebug() << "Empty:" << range.isEmpty();      // false
qDebug() << "Infinite:" << range.isInfinite(); // false
```

### 3. 範囲のチェック

```cpp
FrameRange range(100, 200);

// フレームが範囲内かチェック
bool contains150 = range.contains(150);  // true
bool contains300 = range.contains(300);  // false

// FramePosition で判定
FramePosition pos(150);
bool containsPos = range.contains(pos);  // true

// 他の範囲を完全に含むか
FrameRange subRange(120, 180);
bool containsRange = range.contains(subRange);  // true

// 重なりがあるか
FrameRange other(150, 250);
bool overlaps = range.overlaps(other);  // true

// 接触しているか（重なりまたは隣接）
FrameRange adjacent(201, 300);
bool touches = range.touches(adjacent);  // true
```

### 4. 範囲の操作

```cpp
FrameRange range(100, 200);

// 拡張（両端を広げる）
range.expand(10);  // 90?210

// 縮小（両端を狭める）
range.shrink(10);  // 100?200に戻る

// 片側だけ拡張/縮小
range.expandStart(5);   // 95?200
range.expandEnd(5);     // 95?205
range.shrinkStart(5);   // 100?205
range.shrinkEnd(5);     // 100?200

// 移動（範囲全体をシフト）
range.shift(50);  // 150?250

FrameOffset offset(10);
range.shift(offset);  // 160?260

// コピーを返す操作
FrameRange shifted = range.shifted(10);    // 元は変わらない
FrameRange expanded = range.expanded(10);  // 元は変わらない
FrameRange shrinked = range.shrinked(10);  // 元は変わらない
```

### 5. 範囲演算

```cpp
FrameRange range1(100, 200);
FrameRange range2(150, 250);

// 和集合（両方を含む範囲）
FrameRange united = range1.united(range2);  // 100?250

// 積集合（重なり部分）
FrameRange intersected = range1.intersected(range2);  // 150?200

// 積集合が存在するかチェック
FrameRange result;
if (range1.intersects(range2, result)) {
    qDebug() << "Intersection:" << result.toString();
}
```

### 6. クリッピング

```cpp
FrameRange range(50, 300);
FrameRange bounds(100, 200);

// 範囲を制限
range.clip(bounds);  // range は 100?200 に制限される

// コピーを返す
FrameRange original(50, 300);
FrameRange clipped = original.clipped(bounds);  // 100?200

// フレームを範囲内に丸める
int64_t frame = range.clampFrame(50);   // 100 に丸められる
int64_t frame2 = range.clampFrame(250); // 200 に丸められる
int64_t frame3 = range.clampFrame(150); // 150 のまま

FramePosition pos(50);
FramePosition clamped = range.clampPosition(pos);  // 100
```

### 7. イテレーション

```cpp
FrameRange range(100, 105);

// range-based for ループ
for (int64_t frame : range) {
    qDebug() << "Frame:" << frame;
    // 出力: 100, 101, 102, 103, 104, 105
}

// 全フレーム番号を取得
std::vector<int64_t> allFrames = range.frames();

// 均等にサンプリング（サムネイル生成用）
std::vector<int64_t> samples = range.uniformSample(10);
```

### 8. 正規化

```cpp
// 逆順の範囲（無効）
FrameRange invalid(200, 100);  // start > end

qDebug() << "Valid:" << invalid.isValid();  // false

// 正規化（start と end を入れ替える）
invalid.normalize();
qDebug() << "After normalize:" << invalid.toString();  // [100, 200]

// コピーを返す
FrameRange invalidRange(200, 100);
FrameRange normalized = invalidRange.normalized();  // [100, 200]
```

### 9. 時間変換

```cpp
FrameRange range(0, 300);

// 秒数に変換（30fps）
double seconds = range.durationSeconds(30.0);  // 10秒

// FrameRate で変換
FrameRate rate(30.0);
double seconds2 = range.durationSeconds(rate);  // 10秒

// タイムコード表示
QString timecode = range.toTimecode(30.0);
// "00:00:00:00 - 00:00:10:00"
```

### 10. シリアライズ

```cpp
FrameRange range(100, 200);

// JSON に変換
QJsonObject json = range.toJson();
// {"start": 100, "end": 200}

// JSON から復元
FrameRange restored = FrameRange::fromJson(json);

// 文字列に変換
QString str = range.toString();  // "[100, 200]"

// 文字列から復元
FrameRange parsed = FrameRange::fromString("[100, 200]");
```

### 11. 比較

```cpp
FrameRange range1(100, 200);
FrameRange range2(100, 200);
FrameRange range3(150, 250);

qDebug() << (range1 == range2);  // true
qDebug() << (range1 != range3);  // true
qDebug() << (range1 < range3);   // true (開始位置で比較)
```

## 実際の使用例

### タイムラインのワークエリア

```cpp
class Timeline {
private:
    WorkArea workArea_;  // FrameRange のエイリアス
    
public:
    void setWorkArea(const FrameRange& range) {
        workArea_ = range;
    }
    
    bool isInWorkArea(int64_t frame) const {
        return workArea_.contains(frame);
    }
    
    void renderWorkArea() {
        for (int64_t frame : workArea_) {
            renderFrame(frame);
        }
    }
    
    // 均等間隔でプレビュー生成
    void generatePreviews() {
        auto sampleFrames = workArea_.uniformSample(10);
        for (int64_t frame : sampleFrames) {
            generatePreview(frame);
        }
    }
};
```

### レイヤーの持続時間

```cpp
class Layer {
private:
    LayerRange range_;  // FrameRange のエイリアス
    
public:
    void setInOut(int64_t inPoint, int64_t outPoint) {
        range_.setRange(inPoint, outPoint);
    }
    
    int64_t duration() const {
        return range_.duration();
    }
    
    bool isActive(int64_t frame) const {
        return range_.contains(frame);
    }
    
    void trim(int64_t frames, bool fromStart) {
        if (fromStart) {
            range_.shrinkStart(frames);
        } else {
            range_.shrinkEnd(frames);
        }
    }
    
    void slip(int64_t frames) {
        range_.shift(frames);
    }
};
```

### 複数範囲の管理

```cpp
class CompositionManager {
private:
    std::vector<FrameRange> layerRanges_;
    
public:
    // 全レイヤーの有効範囲を取得
    FrameRange getActiveRange() const {
        if (layerRanges_.empty()) {
            return FrameRange::zero();
        }
        
        FrameRange result = layerRanges_[0];
        for (size_t i = 1; i < layerRanges_.size(); ++i) {
            result = result.united(layerRanges_[i]);
        }
        
        return result;
    }
    
    // 全レイヤーが有効なフレームを取得
    FrameRange getCommonRange() const {
        if (layerRanges_.empty()) {
            return FrameRange::zero();
        }
        
        FrameRange result = layerRanges_[0];
        for (size_t i = 1; i < layerRanges_.size(); ++i) {
            result = result.intersected(layerRanges_[i]);
            if (!result.isValid()) {
                return FrameRange::invalid();
            }
        }
        
        return result;
    }
    
    // 特定フレームで有効なレイヤー数
    int activeLayerCount(int64_t frame) const {
        int count = 0;
        for (const auto& range : layerRanges_) {
            if (range.contains(frame)) {
                count++;
            }
        }
        return count;
    }
};
```

### 再生範囲の管理

```cpp
class PlaybackController {
private:
    PlaybackRange playbackRange_;
    TimelineClock* clock_;
    
public:
    void setPlaybackRange(const FrameRange& range) {
        playbackRange_ = range;
        clock_->setLoopRange(range.start(), range.end());
    }
    
    void play() {
        clock_->setFrame(playbackRange_.start());
        clock_->start();
    }
    
    bool shouldLoop(int64_t currentFrame) const {
        return currentFrame >= playbackRange_.end();
    }
    
    QString getPlaybackInfo() const {
        return QString("Playing: %1 (%2s)")
            .arg(playbackRange_.toString())
            .arg(playbackRange_.durationSeconds(30.0));
    }
};
```

## 他の Frame クラスとの連携

```cpp
// FramePosition と連携
FramePosition pos(150);
FrameRange range(100, 200);
if (range.contains(pos)) {
    qDebug() << "Position is in range";
}

// FrameOffset と連携
FrameOffset offset(50);
FrameRange shifted = range.shifted(offset.value());

// または
range.shift(offset);

// FrameRate と連携
FrameRate rate(30.0);
double seconds = range.durationSeconds(rate);
QString timecode = range.toTimecode(rate);
```

## ベストプラクティス

### 1. 常に正規化されたデータを保持
```cpp
void setRange(const FrameRange& range) {
    range_ = range.normalized();  // 常に start <= end を保証
}
```

### 2. 無効な範囲のチェック
```cpp
FrameRange intersection = range1.intersected(range2);
if (intersection.isValid()) {
    // 積集合が存在する
} else {
    // 重なりなし
}
```

### 3. エイリアスの活用
```cpp
// 目的に応じてエイリアスを使用
WorkArea workArea(0, 1000);        // タイムラインのワークエリア
PlaybackRange playback(100, 200);  // 再生範囲
LayerRange layerDuration(50, 150); // レイヤーの持続時間
```

## 注意事項

- `start > end` の場合、`isValid()` は `false` を返します
- `frames()` メソッドは大きな範囲では大量のメモリを消費する可能性があります（上限100万フレーム）
- イテレーションは範囲の長さに比例して時間がかかります
- `infinite()` 範囲はイテレーションできません

## まとめ

`FrameRange` クラスは：
- ? 他の Frame クラス（Position, Offset, Rate）と完全に統合
- ? 豊富な範囲操作機能
- ? 直感的な API
- ? 動画編集のあらゆるシーンで使用可能

これで、Adobe Premiere Pro や After Effects のような本格的なタイムライン範囲管理が実現できます！
