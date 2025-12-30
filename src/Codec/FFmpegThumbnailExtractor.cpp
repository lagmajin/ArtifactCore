module;

#include <QImage>
#include <QDebug>
module Codec.Thumbnail.FFmpeg;

import std;
import Media.Info;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h> // av_usleep など

}


#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

namespace ArtifactCore {

 class FFmpegThumbnailExtractor::Impl {
 private:

 public:
  
  QImage extractThumbnailInternal(const QString& videoFullPath);
  QImage extractThumbnailFromTimeStamp();
  ThumbnailExtractorResult extractThumbnail(const UniString& videoFullPath);
 };

 QImage FFmpegThumbnailExtractor::Impl::extractThumbnailInternal(const QString& videoFullPath)
 {
  AVFormatContext* fmtCtx = nullptr;
  AVCodecContext* codecCtx = nullptr;
  AVFrame* frame = nullptr;
  AVStream* stream = nullptr;
  AVPacket* packet = nullptr;
  SwsContext* swsCtx = nullptr;
  int videoStreamIndex = -1;
  QImage image;

  if (avformat_open_input(&fmtCtx, videoFullPath.toUtf8().constData(), nullptr, nullptr) != 0) {
   qWarning() << "Failed to open file.";
   return image;
  }

  if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
   qWarning() << "Failed to get stream info.";
   goto cleanup;
  }

  // Find video stream
  videoStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (videoStreamIndex < 0) {
   qWarning() << "No video stream found.";
   goto cleanup;
  }

  stream = fmtCtx->streams[videoStreamIndex];

  // Check for attached thumbnail (e.g. from mp4/mov)
  if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
   AVPacket* thumbPkt = &stream->attached_pic;

   // Try to decode it (e.g. if it's JPEG/PNG)
   QImage thumbImg;
   thumbImg.loadFromData(QByteArray(reinterpret_cast<const char*>(thumbPkt->data), thumbPkt->size));
   if (!thumbImg.isNull()) {
	image = thumbImg;
	goto cleanup;
   }
  }

  // Decode first frame
  {
   const AVCodecParameters* codecPar = stream->codecpar;
   const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
   if (!codec) {
	qWarning() << "Decoder not found.";
	goto cleanup;
   }

   codecCtx = avcodec_alloc_context3(codec);
   if (!codecCtx) goto cleanup;

   if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) goto cleanup;
   if (avcodec_open2(codecCtx, codec, nullptr) < 0) goto cleanup;

   frame = av_frame_alloc();
   packet = av_packet_alloc();

   // Seek to beginning
   av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);

   // Read frames
   while (av_read_frame(fmtCtx, packet) >= 0) {
	if (packet->stream_index != videoStreamIndex) {
	 av_packet_unref(packet);
	 continue;
	}

	if (avcodec_send_packet(codecCtx, packet) == 0) {
	 if (avcodec_receive_frame(codecCtx, frame) == 0) {
	  // Convert to RGB
	  int w = frame->width;
	  int h = frame->height;

	  swsCtx = sws_getContext(w, h, codecCtx->pix_fmt,
	   w, h, AV_PIX_FMT_RGB32,
	   SWS_BICUBIC, nullptr, nullptr, nullptr);

	  QImage img(w, h, QImage::Format_RGB32);
	  uint8_t* dst[] = { img.bits() };
	  int dstLinesize[] = { static_cast<int>(img.bytesPerLine()) };

	  sws_scale(swsCtx, frame->data, frame->linesize, 0, h, dst, dstLinesize);

	  image = img.copy(); // deep copy before cleanup
	  break;
	 }
	}
	av_packet_unref(packet);
   }
  }

 cleanup:
  if (swsCtx) sws_freeContext(swsCtx);
  if (packet) av_packet_free(&packet);
  if (frame) av_frame_free(&frame);
  if (codecCtx) avcodec_free_context(&codecCtx);
  if (fmtCtx) avformat_close_input(&fmtCtx);

  return image;
 }

 ThumbnailExtractorResult FFmpegThumbnailExtractor::Impl::extractThumbnail(const UniString& videoFullPath)
 {
    ThumbnailExtractorResult result;
	AVFormatContext* fmtCtx = nullptr;
	AVCodecContext* codecCtx = nullptr;
	AVFrame* frame = nullptr;
	AVStream* stream = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* swsCtx = nullptr;
	int videoStreamIndex = -1;
	QImage image;
	if (avformat_open_input(&fmtCtx, videoFullPath.toQString().toUtf8().constData(), nullptr, nullptr) != 0) {
	 qWarning() << "Failed to open file.";
	 result.message = ThumbnailExtractorMessage::FileNotFound;
	 return result;
	}

	if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
	 qWarning() << "Failed to get stream info.";
	 result.message = ThumbnailExtractorMessage::NoVideoStream;
	 return result;
	}



   cleanup:
	if (swsCtx) sws_freeContext(swsCtx);
	if (packet) av_packet_free(&packet);
	if (frame) av_frame_free(&frame);
	if (codecCtx) avcodec_free_context(&codecCtx);
	if (fmtCtx) avformat_close_input(&fmtCtx);

	return result;
 }

 FFmpegThumbnailExtractor::FFmpegThumbnailExtractor():impl_(new Impl())
 {

 }

 FFmpegThumbnailExtractor::~FFmpegThumbnailExtractor()
 {
  delete impl_;
 }

 QImage FFmpegThumbnailExtractor::extractThumbnailOld(const QString& videoFilePath)
 {

  return impl_->extractThumbnailInternal(videoFilePath);
 }

 QImage FFmpegThumbnailExtractor::extractThumbnailAtTimestamp(const QString& videoPath, qint64 timestampMs)
 {

  return QImage();
 }

 QImage FFmpegThumbnailExtractor::extractEmbeddedThumbnail(const QString& videoPath)
 {
  return QImage();
 }

ThumbnailExtractorResult FFmpegThumbnailExtractor::extractThumbnail(const UniString& str)
 {

 return   impl_->extractThumbnail(str);
 }

};