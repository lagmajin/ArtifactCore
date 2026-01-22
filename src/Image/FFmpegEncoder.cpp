module;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QFile>
module Encoder.FFmpegEncoder;





 namespace ArtifactCore {
 class FFmpegEncoder::Impl {
 public:
  Impl() {

  }

  ~Impl() {

  }


  void addImageFrame()
  {

  }

  void close() {
   if (!isOpen) return;

   av_write_trailer(fmtCtx);

   if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
	avio_closep(&fmtCtx->pb);
   }

   avcodec_free_context(&codecCtx);
   avformat_free_context(fmtCtx);

   fmtCtx = nullptr;
   codecCtx = nullptr;
   stream = nullptr;
   isOpen = false;
  }

  bool openAudioStream(int sampleRate, int channels) {

   return true;
  }

  void addAudioFrame(const AVFrame* frame) {
   // 音声フレームを送ってパケットを出す処理
  }

 private:
  AVFormatContext* fmtCtx = nullptr;
  AVStream* stream = nullptr;
  AVCodecContext* codecCtx = nullptr;
  AVFrame* frame = nullptr;
  SwsContext* swsCtx = nullptr;
  int frameIndex = 0;
  int width = 0, height = 0;
  bool isOpen = false;

  //audio
  AVStream* audioStream = nullptr;
  AVCodecContext* audioCodecCtx = nullptr;
  AVFrame* audioFrame = nullptr;
  int audioFrameIndex = 0;
 };

 FFmpegEncoder::FFmpegEncoder() : impl_(new Impl())
 {

 }

 FFmpegEncoder::~FFmpegEncoder() {
   delete impl_;
 }


 void FFmpegEncoder::open(const QFile& file)
 {
   //impl->open(file);


 }


 void FFmpegEncoder::close()
 {

 }

 void FFmpegEncoder::addImage(const ImageF32x4_RGBA& image)
 {

 }

}