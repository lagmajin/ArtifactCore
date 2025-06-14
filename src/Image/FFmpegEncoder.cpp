module;


module Encoder:FFMpegEncoder;

#include <QtCore/QFile>

extern "c" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace ArtifactCore {
 class FFMpegEncoder::Impl {
 public:
  Impl() {

  }

  ~Impl() {

  }

  bool FFMpegEncoder::open(const QString& filename, int width, int height, int fps) 
  {

   width = width;
   height = height;

   avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr, filename.toUtf8().constData());
   if (!fmtCtx) return false;

   const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4); // 内蔵コーデック
   stream = avformat_new_stream(fmtCtx, codec);
   stream->id = 0;

   codecCtx = avcodec_alloc_context3(codec);
   codecCtx->width = width;
   codecCtx->height = height;
   codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
   codecCtx->time_base = { 1, fps };
   stream->time_base = codecCtx->time_base;
   codecCtx->bit_rate = 400000;

   if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
	codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if (avcodec_open2(codecCtx, codec, nullptr))
   {


   }
   avcodec_parameters_from_context(stream->codecpar,codecCtx);

   avio_open(&fmtCtx->pb, filename.toUtf8().constData(), AVIO_FLAG_WRITE);
   avformat_write_header(fmtCtx, nullptr);

   frame = av_frame_alloc();
   format = codecCtx->pix_fmt;
   width = width;
   height = height;
   av_frame_get_buffer(frame, 32);

   swsCtx = sws_getContext(width, height, AV_PIX_FMT_BGR24,
	width, height, AV_PIX_FMT_YUV420P,
	SWS_BILINEAR, nullptr, nullptr, nullptr);
   return true;
  



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


 FFMpegEncoder::~FFMpegEncoder()
 {

 }
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