module;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QFile>
module Encoder.FFMpegEncoder;





namespace ArtifactCore {
 class FFMpegEncoder::Impl {
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

 private:
  AVFormatContext* fmtCtx = nullptr;
  AVStream* stream = nullptr;
  AVCodecContext* codecCtx = nullptr;
  AVFrame* frame = nullptr;
  SwsContext* swsCtx = nullptr;
  int frameIndex = 0;
  int width = 0, height = 0;
  bool isOpen = false;
 };

 FFMpegEncoder::FFMpegEncoder() : impl(std::make_unique<Impl>())
 {

 }
 FFMpegEncoder::~FFMpegEncoder() = default;



 void FFMpegEncoder::open(const QFile& file)
 {
   //impl->open(file);


 }


 void FFMpegEncoder::close()
 {

 }

 void FFMpegEncoder::addImage(const ImageF32x4_RGBA& image)
 {

 }

}