# MFFrameExtractor 使用ガイド

## 概要

`MFFrameExtractor` は、Windows Media Foundation を使用して動画ファイルからフレーム（画像）を抽出するクラスです。

## 主な機能

- 特定のフレーム番号や時刻のフレーム抽出
- 複数フレームの一括抽出
- サムネイル生成用の均等間隔フレーム抽出
- 出力フォーマット変換（RGBA, RGB, BGR, BGRA）
- 出力サイズのリサイズ
- ハードウェアアクセラレーション対応
- 統計情報の取得

## 基本的な使い方

### 1. 動画ファイルを開いてフレームを抽出

```cpp
import Codec.MFFrameExtractor;

// フレーム抽出器を作成
MFFrameExtractor extractor;

// 動画ファイルを開く
if (!extractor.open("C:/Videos/sample.mp4")) {
    qWarning() << "Failed to open video:" << extractor.lastError();
    return;
}

// 動画情報を取得
qDebug() << "Width:" << extractor.getWidth();
qDebug() << "Height:" << extractor.getHeight();
qDebug() << "Duration:" << extractor.getDurationSeconds() << "seconds";
qDebug() << "Frame Rate:" << extractor.getFrameRate() << "fps";
qDebug() << "Total Frames:" << extractor.getTotalFrames();

// 100フレーム目を抽出
auto frame = extractor.extractFrameAtIndex(100);
if (frame && frame->isValid()) {
    qDebug() << "Extracted frame:" << frame->width << "x" << frame->height;
    qDebug() << "Data size:" << frame->dataSize() << "bytes";
    
    // QImage に変換
    QImage image(frame->data.data(), 
                frame->width, 
                frame->height, 
                frame->stride,
                QImage::Format_RGBA8888);
    
    // 画像を保存
    image.save("frame_100.png");
}

// ファイルを閉じる
extractor.close();
```

### 2. サムネイル生成（均等間隔でフレーム抽出）

```cpp
MFFrameExtractor extractor;
extractor.open("C:/Videos/sample.mp4");

// 10枚のサムネイルを均等間隔で抽出
auto frames = extractor.extractUniformFrames(10);

for (size_t i = 0; i < frames.size(); ++i) {
    auto& frame = frames[i];
    QImage image(frame->data.data(), 
                frame->width, 
                frame->height, 
                frame->stride,
                QImage::Format_RGBA8888);
    
    QString filename = QString("thumbnail_%1.png").arg(i);
    image.save(filename);
}
```

### 3. 特定の時刻のフレーム抽出

```cpp
MFFrameExtractor extractor;
extractor.open("C:/Videos/sample.mp4");

// 5.5秒の位置のフレームを抽出
auto frame = extractor.extractFrameAtSeconds(5.5);

// または100ナノ秒単位で指定
int64_t timestamp = 55000000; // 5.5秒 = 55,000,000 x 100ns
auto frame2 = extractor.extractFrameAtTime(timestamp);
```

### 4. 範囲内のフレームを抽出（アニメーション用）

```cpp
MFFrameExtractor extractor;
extractor.open("C:/Videos/sample.mp4");

// フレーム100?200を2フレームごとに抽出
auto frames = extractor.extractFrameRange(100, 200, 2);

// アニメーションGIFを作成
QVector<QImage> images;
for (auto& frame : frames) {
    QImage image(frame->data.data(), 
                frame->width, 
                frame->height, 
                frame->stride,
                QImage::Format_RGBA8888);
    images.append(image);
}
```

### 5. 出力フォーマットの変更

```cpp
MFFrameExtractor extractor;

// BGR形式で出力（OpenCV互換）
extractor.setOutputFormat(MFFrameExtractor::OutputFormat::BGR);

extractor.open("C:/Videos/sample.mp4");

auto frame = extractor.extractFrameAtIndex(0);

// OpenCV Mat に変換
cv::Mat mat(frame->height, frame->width, CV_8UC3, frame->data.data());
cv::imwrite("frame.jpg", mat);
```

### 6. 出力サイズのリサイズ

```cpp
MFFrameExtractor extractor;

// 640x360にリサイズして出力
extractor.setOutputSize(640, 360);

extractor.open("C:/Videos/sample.mp4");

auto frame = extractor.extractFrameAtIndex(0);
// frame は 640x360 のサイズで返される

// オリジナルサイズに戻す
extractor.clearOutputSize();
```

### 7. ハードウェアアクセラレーション

```cpp
MFFrameExtractor extractor;

// ハードウェアアクセラレーションを有効化（デフォルト）
extractor.setHardwareAcceleration(true);

// GPU でデコードされる（対応している場合）
extractor.open("C:/Videos/sample.mp4");

qDebug() << "Hardware acceleration:" 
         << (extractor.isHardwareAccelerationEnabled() ? "Enabled" : "Disabled");
```

### 8. 複数の特定フレームを抽出

```cpp
MFFrameExtractor extractor;
extractor.open("C:/Videos/sample.mp4");

// 抽出したいフレーム番号のリスト
std::vector<int64_t> frameIndices = {0, 100, 500, 1000, 1500};

auto frames = extractor.extractFrames(frameIndices);

for (size_t i = 0; i < frames.size(); ++i) {
    auto& frame = frames[i];
    qDebug() << "Frame" << frameIndices[i] 
             << "at timestamp" << frame->timestamp
             << "(" << (frame->timestamp / 10000000.0) << "seconds)";
}
```

## エラーハンドリング

```cpp
MFFrameExtractor extractor;

if (!extractor.open("invalid_file.mp4")) {
    qWarning() << "Error:" << extractor.lastError();
    return;
}

auto frame = extractor.extractFrameAtIndex(999999);
if (!frame) {
    qWarning() << "Frame extraction failed:" << extractor.lastError();
}

// エラーをクリア
extractor.clearError();
```

## 統計情報の取得

```cpp
MFFrameExtractor extractor;
extractor.open("C:/Videos/sample.mp4");

// 複数フレームを抽出
for (int i = 0; i < 100; ++i) {
    extractor.extractFrameAtIndex(i * 10);
}

// 統計情報を取得
auto stats = extractor.getStatistics();
qDebug() << "Total frames extracted:" << stats.totalFramesExtracted;
qDebug() << "Total bytes processed:" << stats.totalBytesProcessed;
qDebug() << "Average extraction time:" << stats.averageExtractionTimeMs << "ms";

// 統計をリセット
extractor.resetStatistics();
```

## タイムラインプレビュー用の使用例

```cpp
class TimelinePreviewGenerator {
private:
    MFFrameExtractor extractor_;
    
public:
    bool loadVideo(const QString& path) {
        extractor_.setOutputSize(160, 90);  // プレビューサイズ
        extractor_.setOutputFormat(MFFrameExtractor::OutputFormat::RGBA);
        return extractor_.open(path);
    }
    
    QVector<QImage> generatePreviews(int count) {
        auto frames = extractor_.extractUniformFrames(count);
        
        QVector<QImage> images;
        for (auto& frame : frames) {
            QImage image(frame->data.data(), 
                        frame->width, 
                        frame->height, 
                        frame->stride,
                        QImage::Format_RGBA8888);
            images.append(image.copy());  // ディープコピー
        }
        
        return images;
    }
    
    QImage getFrameAt(double seconds) {
        auto frame = extractor_.extractFrameAtSeconds(seconds);
        if (!frame) return QImage();
        
        return QImage(frame->data.data(), 
                     frame->width, 
                     frame->height, 
                     frame->stride,
                     QImage::Format_RGBA8888).copy();
    }
};
```

## 注意事項

### メモリ管理
- `ExtractedFrame` は `std::unique_ptr` で返されるため、自動的にメモリ管理されます
- `QImage` を作成する際は、フレームデータをコピーするか、フレームのライフタイムに注意してください

### スレッドセーフ
- `MFFrameExtractor` は **スレッドセーフではありません**
- 複数スレッドから使用する場合は、インスタンスを分けるか、ミューテックスで保護してください

### パフォーマンス
- ハードウェアアクセラレーションを有効にすることで、デコード速度が大幅に向上します
- 大量のフレームを抽出する場合は、`extractFrames()` を使用してまとめて抽出することを推奨

### 対応フォーマット
- Windows Media Foundation が対応しているすべての動画形式に対応
- 一般的: MP4, AVI, MKV, MOV, WMV, FLV など

## 旧クラス（MFEncoder）からの移行

```cpp
// 旧
MFEncoder encoder;
// ...

// 新（互換性エイリアス）
MFEncoder extractor;  // MFFrameExtractor のエイリアス
extractor.open("video.mp4");
auto frame = extractor.extractFrameAtIndex(0);
```

旧 `MFEncoder` は `MFFrameExtractor` のエイリアスとして定義されているため、既存コードも動作します。
