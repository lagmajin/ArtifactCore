﻿module;

#include <QDebug>
#include <QString>
#include <QImage>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>

}

module Codec.FFMpegDecoder;

import std;


namespace ArtifactCore {

 static std::string av_strerror_string(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE];
  av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, errnum);
  return std::string(errbuf);
 }

 class FFMpegDecoder::Impl {
 private:
  AVFormatContext* formatContext = nullptr;
  AVCodecContext* codecContext = nullptr;
  int              videoStreamIndex = 0;
  AVPacket* packet = nullptr;
  AVFrame* frame = nullptr;
  SwsContext* swsCtx_ = nullptr;
 public:
  ~Impl()
  {
   closeFile();
  }
  bool openFile(const QString& path);
  QImage decodeNextVideoFrame();
  void closeFile();
  void seekByFrameNumber(int64_t frameNumber);

  // タイムスタンプ（ミリ秒）でシーク
  void seekByTimestamp(int64_t timestampMs);
 };

 bool FFMpegDecoder::Impl::openFile(const QString& path)
 {
  closeFile();
  // 1. AVFormatContextを初期化
  formatContext = avformat_alloc_context();
  if (!formatContext) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to allocate AVFormatContext.";
   return false;
  }

  if (avformat_open_input(&formatContext, path.toUtf8().constData(), nullptr, nullptr) < 0) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to open input file:" << path << av_strerror_string(AVERROR_EOF).c_str(); // ERROR_EOFはファイルが見つからない可能性も
   avformat_free_context(formatContext);
   formatContext = nullptr;
   return false;
  }

  if (avformat_find_stream_info(formatContext, nullptr) < 0) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to find stream info.";
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  videoStreamIndex = -1;
  AVCodecParameters* codecParameters = nullptr;
  for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
   if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	videoStreamIndex = i;
	codecParameters = formatContext->streams[i]->codecpar;
	break;
   }
  }

  if (videoStreamIndex == -1) {
   qWarning() << "FFmpegDecoder::Impl::openFile: No video stream found.";
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  // 5. コーデックを見つける
  const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
  if (!codec) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Codec not found for ID:" << codecParameters->codec_id;
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  // 6. コーデックコンテキストを初期化
  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to allocate AVCodecContext.";
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  // 7. コーデックパラメータをコーデックコンテキストにコピー
  if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to copy codec parameters to context.";
   avcodec_free_context(&codecContext);
   codecContext = nullptr;
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  // 8. コーデックを開く
  if (avcodec_open2(codecContext, codec, nullptr) < 0) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to open codec.";
   avcodec_free_context(&codecContext);
   codecContext = nullptr;
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  // 9. パケットとフレームを割り当て
  packet = av_packet_alloc();
  if (!packet) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to allocate AVPacket.";
   avcodec_free_context(&codecContext);
   codecContext = nullptr;
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }
  frame = av_frame_alloc();
  if (!frame) {
   qWarning() << "FFmpegDecoder::Impl::openFile: Failed to allocate AVFrame.";
   av_packet_free(&packet);
   packet = nullptr;
   avcodec_free_context(&codecContext);
   codecContext = nullptr;
   avformat_close_input(&formatContext);
   formatContext = nullptr;
   return false;
  }

  qDebug() << "FFmpegDecoder::Impl::openFile: Successfully opened file:" << path;
  return true; // 正常に初期化完了


 }


 void FFMpegDecoder::Impl::closeFile()
 {
  if (packet) {
   av_packet_free(&packet);
   packet = nullptr;
  }
  if (frame) {
   av_frame_free(&frame);
   frame = nullptr;
  }
  if (codecContext) {
   //avcodec_close(codecContext); // avcodec_free_contextは内部でavcodec_closeを呼ぶが、明示的に。
   avcodec_free_context(&codecContext);
   codecContext = nullptr;
  }
  if (formatContext) {
   avformat_close_input(&formatContext);
   formatContext = nullptr;
  }
  videoStreamIndex = -1;
  qDebug() << "FFmpegDecoder::Impl::closeFile: Resources released.";
 }

 QImage FFMpegDecoder::Impl::decodeNextVideoFrame()
 {
  while (av_read_frame(formatContext, packet) >= 0) {
   if (packet->stream_index == videoStreamIndex) {
	avcodec_send_packet(codecContext, packet);
	if (avcodec_receive_frame(codecContext, frame) == 0) {
	 // RGBへ変換
	 uint8_t* dstData[4];
	 int dstLinesize[4];
	 QImage img(codecContext->width, codecContext->height, QImage::Format_RGB888);
	 av_image_fill_arrays(dstData, dstLinesize, img.bits(), AV_PIX_FMT_RGB24, img.width(), img.height(), 1);
	 sws_scale(swsCtx_, frame->data, frame->linesize, 0, codecContext->height, dstData, dstLinesize);
	 av_packet_unref(packet);
	 return img;
	}
   }
   av_packet_unref(packet);
  }
  return QImage(); // 失敗
 }

 void FFMpegDecoder::Impl::seekByFrameNumber(int64_t frameNumber)
 {
  if (!formatContext || videoStreamIndex < 0)
   return;

  const AVStream* stream = formatContext->streams[videoStreamIndex];

  // フレーム番号 → 時間（pts）に変換
  int64_t timestamp = av_rescale_q(frameNumber, AVRational{ 1, stream->r_frame_rate.num }, stream->time_base);

  int ret = av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
   qWarning() << "seekByFrameNumber: av_seek_frame failed.";
   return;
  }

  avcodec_flush_buffers(codecContext);
 }

 void FFMpegDecoder::Impl::seekByTimestamp(int64_t timestampMs)
 {
  if (!formatContext || videoStreamIndex < 0)
   return;

  const AVRational timeBase = formatContext->streams[videoStreamIndex]->time_base;

  // ミリ秒 → streamのtime_base単位に変換（秒単位なら ×1000 / AV_TIME_BASE）
  int64_t timestamp = av_rescale_q(timestampMs, AVRational{ 1, 1000 }, timeBase);

  // フラグ：近くのキーフレームへ
  int ret = av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);

  if (ret < 0) {
   qWarning() << "seekByTimestamp: av_seek_frame failed.";
   return;
  }

  avcodec_flush_buffers(codecContext);
 }

FFMpegDecoder::FFMpegDecoder() noexcept:impl_(new Impl())
{

}

FFMpegDecoder::~FFMpegDecoder()
{

}

bool FFMpegDecoder::openFile(const QString& path)
{
 if (!impl_) {
  //impl_ = std::make_unique<Impl>(); // 万が一pimplがnullptrの場合の再初期化
 }
 return impl_->openFile(path);


}

void FFMpegDecoder::closeFile()
{
 if (impl_) {
  impl_->closeFile();
 }
}

QImage FFMpegDecoder::decodeNextVideoFrame()
{
 return impl_->decodeNextVideoFrame();
}

};