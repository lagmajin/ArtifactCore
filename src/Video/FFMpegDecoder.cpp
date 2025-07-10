module;
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <QDebug>
#include <QString>
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

 public:
  bool openFile(const QString& path);
  AVFrame* decodeNextVideoFrame();
  void closeFile();
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
   avcodec_close(codecContext); // avcodec_free_contextは内部でavcodec_closeを呼ぶが、明示的に。
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

AVFrame* FFMpegDecoder::decodeNextVideoFrame()
{
 if (impl_) return nullptr;
 return impl_->decodeNextVideoFrame();
}

};