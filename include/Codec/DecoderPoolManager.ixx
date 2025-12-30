module;
export module DecoderPoolManager;

export namespace ArtifactCore {
 
 class DecoderPoolManager {
 public:
  DecoderPoolManager();
  ~DecoderPoolManager();
  // プールからデコーダーを取得
  // FFMpegAudioDecoder* acquireDecoder(const QString& path);
  // デコーダーをプールに返却
  // void releaseDecoder(FFMpegAudioDecoder* decoder);
 private:
  // QVector<FFMpegAudioDecoder*> decoderPool_;
 };
}