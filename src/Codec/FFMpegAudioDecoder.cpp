module;
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}
#include <QVector>
module Media.Encoder.FFmpegAudioDecoder;

import std;
import Utils.String.UniString;
import Audio.RingBuffer;
import Audio.Segment;
import Audio.BufferQueue;

namespace ArtifactCore
{
 class FFmpegAudioDecoder::Impl {
 private:

  AVFormatContext* fmtCtx_ = nullptr;
  AVCodecContext* codecCtx_ = nullptr;
  int audioStreamIndex_ = -1;
  //SwrContext* swrCtx_ = nullptr;
  AVPacket* pkt_ = nullptr;
  AVFrame* frame_ = nullptr;
  SwrContext* swrCtx_ = nullptr;
  //double seekTargetSeconds_ = -1.0;
 public:
  double seekTargetSeconds_ = -1.0;
  Impl();
  ~Impl();
  bool openFile(const QString& path);
  void closeFile();
  void setupResampler();
  void seek(double seek);
  bool decodeNextFrame(AudioBufferQueue& queue);
  void flush();
  bool isSameFile(const QString& path);
  bool isSameFile(const UniString& path);
 };

 FFmpegAudioDecoder::Impl::Impl()
 {

 }

 bool FFmpegAudioDecoder::Impl::openFile(const QString& path)
 {
  closeFile();
  if (avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr, nullptr) != 0)
  {
   return false;
  }
  if (avformat_find_stream_info(fmtCtx_, nullptr) < 0)
  {
   return false;
  }
  const AVCodec* codec = nullptr;
  audioStreamIndex_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  if (audioStreamIndex_ < 0) return false;

  codecCtx_ = avcodec_alloc_context3(codec);
  if (!codecCtx_) return false;
  avcodec_parameters_to_context(codecCtx_, fmtCtx_->streams[audioStreamIndex_]->codecpar);

  if (avcodec_open2(codecCtx_, codec, nullptr) < 0)
  {
   return false;
  }


  setupResampler();

  return true;
 }

 void FFmpegAudioDecoder::Impl::closeFile()
 {

  if (codecCtx_) {
   avcodec_free_context(&codecCtx_);
   codecCtx_ = nullptr;
  }
  if (fmtCtx_) {
   avformat_close_input(&fmtCtx_);
   fmtCtx_ = nullptr;
  }
  audioStreamIndex_ = -1;
 }

 void FFmpegAudioDecoder::Impl::seek(double seconds) {
  if (!fmtCtx_ || audioStreamIndex_ < 0) return;

  AVRational tb = fmtCtx_->streams[audioStreamIndex_]->time_base;
  int64_t target_ts = static_cast<int64_t>(seconds / av_q2d(tb));

  if (av_seek_frame(fmtCtx_, audioStreamIndex_, target_ts, AVSEEK_FLAG_BACKWARD) >= 0) {
   avcodec_flush_buffers(codecCtx_);

   // 【重要】シーク直後の「読み飛ばし」が必要なターゲット時間を記録
   seekTargetSeconds_ = seconds;

   // RingBufferのクリアは、ここか、この直後に呼び出し元で行う
   // ringBuffer_->clear(); 
  }
 }

 void FFmpegAudioDecoder::Impl::setupResampler()
 {
  swr_alloc_set_opts2(&swrCtx_,
   &codecCtx_->ch_layout, AV_SAMPLE_FMT_FLTP, codecCtx_->sample_rate, // 出力 (Planar Float)
   &codecCtx_->ch_layout, codecCtx_->sample_fmt, codecCtx_->sample_rate, // 入力
   0, nullptr);
  swr_init(swrCtx_);
 }

 bool FFmpegAudioDecoder::Impl::decodeNextFrame(AudioBufferQueue& queue)
 {// 1. パケットの読み込み
  if (av_read_frame(fmtCtx_, pkt_) < 0) return false; // 終端

  if (pkt_->stream_index == audioStreamIndex_) {
   // 2. デコーダへ送信
   if (avcodec_send_packet(codecCtx_, pkt_) >= 0) {
	while (avcodec_receive_frame(codecCtx_, frame_) >= 0) {

	 // 3. リサンプリング後の出力先を準備 (AudioSegment)
	 AudioSegment segment;
	 segment.sampleRate = codecCtx_->sample_rate;
	 //segment.layout = mapToYourLayout(codecCtx_->ch_layout); // レイアウト変換関数
	 segment.startFrame = frame_->pts; // タイムスタンプ保存

	 int nb_channels = codecCtx_->ch_layout.nb_channels;
	 segment.channelData.resize(nb_channels);
	 for (int i = 0; i < nb_channels; ++i) {
	  segment.channelData[i].resize(frame_->nb_samples);
	 }

	 // 4. SwrContextで変換実行
	 // 出力バッファのポインタ配列を作成
	 uint8_t* out_ptrs[64]; // 十分な数
	 for (int i = 0; i < nb_channels; ++i) {
	  out_ptrs[i] = reinterpret_cast<uint8_t*>(segment.channelData[i].data());
	 }

	 swr_convert(swrCtx_,
	  out_ptrs, frame_->nb_samples,
	  (const uint8_t**)frame_->data, frame_->nb_samples);

	 // 5. キューへプッシュ
	 queue.push(segment);
	}
   }
  }
  av_packet_unref(pkt_);
  return true;


  return true;
 }

 FFmpegAudioDecoder::Impl::~Impl()
 {
  closeFile(); // コーデックとコンテキストを閉じる
  if (pkt_) av_packet_free(&pkt_);
  if (frame_) av_frame_free(&frame_);
  if (swrCtx_) swr_free(&swrCtx_);
 }

 void FFmpegAudioDecoder::Impl::flush()
 {
  if (codecCtx_) {
   avcodec_flush_buffers(codecCtx_);
  }
  // SwrContextのリセットも必要
  if (swrCtx_) {
   swr_init(swrCtx_);
  }
  seekTargetSeconds_ = -1.0;
 }

 bool FFmpegAudioDecoder::Impl::isSameFile(const QString& path)
 {
  if (!fmtCtx_ || !fmtCtx_->url) return false;
  // FFmpeg内部のパスと、渡されたパスを比較
  return QString::fromUtf8(fmtCtx_->url) == path;
 }

 bool FFmpegAudioDecoder::Impl::isSameFile(const UniString& path)
 {

  return true;
 }

 FFmpegAudioDecoder::FFmpegAudioDecoder() :impl_(new Impl())
 {

 }

 FFmpegAudioDecoder::~FFmpegAudioDecoder()
 {
  delete impl_;
 }

 bool FFmpegAudioDecoder::openFile(const QString& path)
 {
  return impl_->openFile(path);
 }

 void FFmpegAudioDecoder::closeFile()
 {
  impl_->closeFile();
 }

 void FFmpegAudioDecoder::seek(double seek)
 {

 }

 void FFmpegAudioDecoder::fillCacheAsync(double start, double end)
 {

 }

 void FFmpegAudioDecoder::flush()
 {
  //impl_->flush();
 }

 bool FFmpegAudioDecoder::isSameFile(const UniString& path) const
 {
  return impl_->isSameFile(path);
 }

};