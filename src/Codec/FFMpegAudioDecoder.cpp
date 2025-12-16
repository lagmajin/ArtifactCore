module;
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>

}
#include <QVector>
module Media.Encoder.FFMpegAudioDecoder;

namespace ArtifactCore
{
 class FFMpegAudioDecoder::Impl {
 private:

  AVFormatContext* fmtCtx_ = nullptr;
  AVCodecContext* codecCtx_ = nullptr;
  int audioStreamIndex_ = -1;
  //SwrContext* swrCtx_ = nullptr;
  AVPacket* pkt_ = nullptr;
  AVFrame* frame_ = nullptr;
//int audioStreamIndex_ = -1;
 public:
  Impl();
  ~Impl() = default;
  bool openFile(const QString& path);
  void closeFile();
  void seek(double seek);
 };

 FFMpegAudioDecoder::Impl::Impl()
 {

 }

 bool FFMpegAudioDecoder::Impl::openFile(const QString& path)
 {
  closeFile();
  if (avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr, nullptr) != 0)
   return false;
  if (avformat_find_stream_info(fmtCtx_, nullptr) < 0)
   return false;
  return false;
 }

 void FFMpegAudioDecoder::Impl::closeFile()
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

 void FFMpegAudioDecoder::Impl::seek(double seek)
 {
  if (!fmtCtx_ || audioStreamIndex_ < 0) return;
  int64_t ts = static_cast<int64_t>(seek * AV_TIME_BASE);
  av_seek_frame(fmtCtx_, audioStreamIndex_, ts, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(codecCtx_);
 }

 FFMpegAudioDecoder::FFMpegAudioDecoder() :impl_(new Impl())
 {

 }

 FFMpegAudioDecoder::~FFMpegAudioDecoder()
 {
  delete impl_;
 }

 bool FFMpegAudioDecoder::openFile(const QString& path)
 {
  return impl_->openFile(path);
 }

 void FFMpegAudioDecoder::closeFile()
 {
  impl_->closeFile();
 }

 void FFMpegAudioDecoder::seek(double seek)
 {

 }

};