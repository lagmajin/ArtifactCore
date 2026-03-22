# FFmpegEncoder Milestone

日付：2026-03-17  
目標：`ArtifactCore::FFmpegEncoder` を実用的なビデオエンコーダーとして完成させる

---

## Goal

- H.264 / H.265 / ProRes / PNG などの主要コーデックでエンコード可能
- MP4 / MOV / AVI / WebM などの主要コンテナ形式に対応
- 連番画像出力（PNG/JPEG/EXR/TIFF）もサポート
- `ArtifactRenderQueueService` から呼び出して実際にファイル出力できる

---

## Definition of Done

- [ ] `FFmpegEncoder` が単一コーデック・単一コンテナでエンコード可能
- [ ] 複数コーデック（ビデオ＋オーディオ）のマルチストリーム対応
- [ ] 連番画像出力対応
- [ ] `ArtifactRenderQueueService` のレンダリングパイプラインと統合
- [ ] エラーハンドリングと進捗報告が機能する
- [ ] 単体テストまたは検証用サンプルが動作する

---

## Milestones

### M-FFMPEG-1: Foundation - 最小限の H.264 MP4 エンコーダー

**目標**: 単一コーデック（H.264）・単一コンテナ（MP4）で最低限エンコード可能にする

**完了条件**:
- [ ] `open()` で出力ファイルパスとコーデック/コンテナを受け取れる
- [ ] `addImage()` で RGBA 画像を 1 フレームずつ追加できる
- [ ] `close()` でファイルを確定して閉じられる
- [ ] 固定パラメーター（1920x1080, 30fps, 8000kbps）で H.264 MP4 出力可能

**API 案**:
```cpp
class FFmpegEncoder {
public:
    // 初期化
    bool open(const QString& outputPath, 
              int width, int height, 
              double fps, int bitrateKbps);
    
    // フレーム追加
    bool addImage(const ImageF32x4_RGBA& image);
    
    // 完了
    void close();
};
```

**主な実装タスク**:
1. FFmpeg ライブラリの初期化（`avformat_alloc_output_context2`）
2. H.264 コーデックの検索とコンテキスト設定（`avcodec_find_encoder`）
3. ストリーム作成とパラメーター設定
4. 画像フォーマット変換（RGBA → NV12/YUV420P）
5. エンコードループ（`avcodec_send_frame` / `avcodec_receive_packet`）
6. パケットの_muxing_（`av_interleaved_write_frame`）

**見積**: 4-8 時間

---

### M-FFMPEG-2: Codec / Container Flexibility

**目標**: 複数コーデック・複数コンテナを選択可能にする

**完了条件**:
- [ ] コーデック指定（`h264`, `h265`, `prores`, `mjpeg`）
- [ ] コンテナ指定（`mp4`, `mov`, `avi`, `webm`）
- [ ] コーデック別パラメーター設定（ビットレート、プロファイル等）
- [ ] 無効な組み合わせはエラーで弾く

**API 案**:
```cpp
struct FFmpegEncoderSettings {
    QString videoCodec;      // "h264", "h265", "prores", "png"
    QString container;       // "mp4", "mov", "avi", "webm"
    int width;
    int height;
    double fps;
    int bitrateKbps;
    QString preset;          // "ultrafast", "medium", "slow"
    int crf;                 // 品質（0-51, 低ほど高品質）
};

bool open(const QString& outputPath, const FFmpegEncoderSettings& settings);
```

**主な実装タスク**:
1. コーデック検索関数の一般化（`avcodec_find_encoder_by_name`）
2. コンテナフォーマットの自動選択（`av_guess_format`）
3. コーデック別オプション設定（`av_opt_set`）
4. エラーハンドリングの強化

**見積**: 3-5 時間

---

### M-FFMPEG-3: Image Sequence Output

**目標**: 連番画像出力（PNG/JPEG/EXR/TIFF）に対応

**完了条件**:
- [ ] 連番画像フォーマット指定（`png`, `jpeg`, `exr`, `tiff`）
- [ ] ファイル命名規則（`frame_0001.png`, `frame_0002.png`...）
- [ ] 16bit/32bit 出力対応（EXR の場合）
- [ ] 圧縮レベル設定（JPEG 品質等）

**API 案**:
```cpp
struct FFmpegImageSequenceSettings {
    QString format;          // "png", "jpeg", "exr", "tiff"
    int width;
    int height;
    int startFrame;          // 開始フレーム番号
    int padding;             // ゼロ埋め桁数（4 なら 0001）
    int jpegQuality;         // JPEG 品質（1-100）
    bool is16bit;            // 16bit 出力（PNG/TIFF）
};

bool openImageSequence(const QString& outputPathPattern, 
                       const FFmpegImageSequenceSettings& settings);
```

**主な実装タスク**:
1. 画像フォーマット別エンコーダー設定
2. ファイル名パターン生成（`%04d` 展開）
3. 画像データ形式変換（RGBA → 各フォーマット）
4. 16bit/32bit 対応（`AV_PIX_FMT_RGB48LE` / `AV_PIX_FMT_RGBF32LE`）

**見積**: 3-4 時間

---

### M-FFMPEG-4: Audio Stream Support

**目標**: ビデオ＋オーディオのマルチストリーム対応

**完了条件**:
- [ ] オーディオコーデック指定（`aac`, `mp3`, `pcm_s16le`）
- [ ] サンプリングレート・チャンネル設定
- [ ] ビデオ/オーディオのインターリーブ
- [ ] 音声なしモードも継続サポート

**API 案**:
```cpp
struct FFmpegAudioSettings {
    QString audioCodec;      // "aac", "mp3", "pcm_s16le"
    int sampleRate;          // 44100, 48000
    int channels;            // 1 (mono), 2 (stereo)
    int bitrateKbps;         // 128, 192, 320
};

bool open(const QString& outputPath, 
          const FFmpegEncoderSettings& videoSettings,
          const FFmpegAudioSettings& audioSettings);

bool addAudioFrame(const AudioBuffer& audio);  // 音声フレーム追加
```

**主な実装タスク**:
1. オーディオストリーム作成
2. 音声フォーマット変換（`swr_convert`）
3. ビデオ/オーディオのタイミング同期
4. インターリーブ処理（`av_interleaved_write_frame`）

**見積**: 4-6 時間

---

### M-FFMPEG-5: RenderQueue Integration

**目標**: `ArtifactRenderQueueService` と統合して実際にレンダリング可能に

**完了条件**:
- [ ] `ArtifactRenderQueueService` が `FFmpegEncoder` を呼び出す
- [ ] 進捗報告（`progress` シグナル）
- [ ] エラーメッセージの伝播
- [ ] キャンセル処理
- [ ] 出力先ディレクトリ作成

**実装イメージ**:
```cpp
// ArtifactRenderQueueService.cppm 内
bool RenderQueueManager::renderFrameSequence(
    const ArtifactRenderJob& job,
    std::function<QImage(int)> renderFrameCallback)
{
    FFmpegEncoder encoder;
    
    FFmpegEncoderSettings settings;
    settings.videoCodec = job.codec;
    settings.container = job.outputFormat;
    settings.width = job.resolutionWidth;
    settings.height = job.resolutionHeight;
    settings.fps = job.frameRate;
    settings.bitrateKbps = job.bitrate;
    
    if (!encoder.open(job.outputPath, settings)) {
        job.errorMessage = encoder.lastError();
        return false;
    }
    
    for (int frame = job.startFrame; frame <= job.endFrame; ++frame) {
        QImage rendered = renderFrameCallback(frame);
        ImageF32x4_RGBA image = convertToFloatRGBA(rendered);
        
        if (!encoder.addImage(image)) {
            job.errorMessage = encoder.lastError();
            encoder.close();
            return false;
        }
        
        // 進捗報告
        int progress = (frame - job.startFrame) * 100 / 
                       (job.endFrame - job.startFrame);
        if (jobProgressChanged) jobProgressChanged(jobIndex, progress);
    }
    
    encoder.close();
    return true;
}
```

**主な実装タスク**:
1. `ArtifactRenderQueueService` のレンダリングループ修正
2. `FFmpegEncoder` の呼び出しラッパー実装
3. 進捗コールバックの実装
4. エラーハンドリングの統一

**見積**: 3-5 時間

---

### M-FFMPEG-6: Testing & Validation

**目標**: 検証用サンプルと単体テストを整備

**完了条件**:
- [ ] 検証用コマンドラインツール（`ffmpeg_test_encoder`）
- [ ] 単体テスト（H.264 MP4 出力テスト）
- [ ] 連番画像出力テスト
- [ ] エラーケーステスト（無効なパラメーター等）

**テスト項目**:
```cpp
TEST(FFmpegEncoder, H264Mp4Encoding) {
    FFmpegEncoder encoder;
    FFmpegEncoderSettings settings;
    settings.videoCodec = "h264";
    settings.container = "mp4";
    settings.width = 1920;
    settings.height = 1080;
    settings.fps = 30.0;
    settings.bitrateKbps = 8000;
    
    ASSERT_TRUE(encoder.open("test_output.mp4", settings));
    
    // ダミーフレームを 100 フレーム追加
    for (int i = 0; i < 100; ++i) {
        ImageF32x4_RGBA frame = generateTestPattern(i);
        ASSERT_TRUE(encoder.addImage(frame));
    }
    
    encoder.close();
    
    // 出力ファイルが存在するか確認
    ASSERT_TRUE(QFile::exists("test_output.mp4"));
}
```

**見積**: 2-4 時間

---

## Recommended Order

1. **M-FFMPEG-1: Foundation** - 最小限の H.264 MP4 エンコーダー
2. **M-FFMPEG-2: Codec / Container Flexibility** - 複数コーデック対応
3. **M-FFMPEG-3: Image Sequence Output** - 連番画像出力
4. **M-FFMPEG-5: RenderQueue Integration** - 統合（※M-FFMPEG-4 より優先）
5. **M-FFMPEG-4: Audio Stream Support** - オーディオ（※後回し可能）
6. **M-FFMPEG-6: Testing & Validation** - テスト整備

---

## Total Estimate

| フェーズ | 見積時間 |
| --- | --- |
| M-FFMPEG-1 | 4-8h |
| M-FFMPEG-2 | 3-5h |
| M-FFMPEG-3 | 3-4h |
| M-FFMPEG-4 | 4-6h |
| M-FFMPEG-5 | 3-5h |
| M-FFMPEG-6 | 2-4h |
| **合計** | **19-32h** |

---

## Dependencies

### 外部ライブラリ

```cmake
# CMakeLists.txt または vcpkg.json に追加
find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil swscale)

target_link_libraries(ArtifactCore PRIVATE 
    FFmpeg::avcodec
    FFmpeg::avformat
    FFmpeg::avutil
    FFmpeg::swscale
)
```

### vcpkg 依存

```json
{
  "dependencies": [
    "ffmpeg"
  ]
}
```

---

## Risk & Notes

### リスク

1. **FFmpeg の ABI 非互換** - バージョン固定が必要
2. **コーデックのライセンス** - H.264/H.265 は特許問題あり（ProRes は Apple 製）
3. **エンコード速度** - ソフトウェアエンコードは遅い（NVENC 等ハードウェア検討も）

### 備考

- 最初のバージョンはソフトウェアエンコード（libx264）で十分
- ハードウェアエンコード（NVENC/AMF/QSV）は後続マイルストーンで
- 進捗報告は `std::function<void(int progress)>` コールバックで柔軟に
- エラーメッセージは `lastError()` で取得可能に

---

## First Step

**M-FFMPEG-1** から着手。

最初にやること：
1. `FFmpegEncoder.cppm` に H.264 MP4 エンコードのスケルトン実装
2. 固定パラメーターで 1 フレーム出力できることを確認
3. 徐々にパラメーターを可変化
