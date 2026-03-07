module;

#include <QImage>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h> // av_usleep など

}
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")
module Codec.Thumbnail.FFmpeg;




import Media.Info;





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
    result.success = false;
    result.message = ThumbnailExtractorMessage::UnknownError;

    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVStream* stream = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* swsCtx = nullptr;
    int videoStreamIndex = -1;

    // Open file
    if (avformat_open_input(&fmtCtx, videoFullPath.toQString().toUtf8().constData(), nullptr, nullptr) != 0) {
        qWarning() << "FFmpegThumbnailExtractor: Failed to open file:" << videoFullPath.toQString();
        result.message = ThumbnailExtractorMessage::FileNotFound;
        return result;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        qWarning() << "FFmpegThumbnailExtractor: Failed to find stream info.";
        result.message = ThumbnailExtractorMessage::NoVideoStream;
        goto cleanup;
    }

    // Find best video stream
    videoStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        qWarning() << "FFmpegThumbnailExtractor: No video stream found.";
        result.message = ThumbnailExtractorMessage::NoVideoStream;
        goto cleanup;
    }
    
    stream = fmtCtx->streams[videoStreamIndex];

    // Priority 1: Check for attached pictures/cover art (e.g., MP3 with cover or MP4 with poster)
    if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
        AVPacket* thumbPkt = &stream->attached_pic;
        QImage thumbImg;
        if (thumbImg.loadFromData(QByteArray(reinterpret_cast<const char*>(thumbPkt->data), thumbPkt->size))) {
            result.image = thumbImg;
            result.success = true;
            result.message = ThumbnailExtractorMessage::Success;
            goto cleanup;
        }
    }

    // Attempt to decode a frame from the video stream
    {
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            qWarning() << "FFmpegThumbnailExtractor: Decoder not found.";
            result.message = ThumbnailExtractorMessage::DecoderNotFound;
            goto cleanup;
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            goto cleanup;
        }

        if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0) {
            goto cleanup;
        }

        // Multithreading for decode (using slice or frame threading if supported)
        codecCtx->thread_count = 0; // Auto
        if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS)
            codecCtx->thread_type = FF_THREAD_FRAME;
        else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS)
            codecCtx->thread_type = FF_THREAD_SLICE;

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            result.message = ThumbnailExtractorMessage::DecodeError;
            goto cleanup;
        }

        frame = av_frame_alloc();
        packet = av_packet_alloc();
        if (!frame || !packet) goto cleanup;

        // Try seeking to 10% of the video duration, or 1 second, to avoid black frames.
        if (fmtCtx->duration > 0 && fmtCtx->duration != AV_NOPTS_VALUE) {
            int64_t targetTimestamp = fmtCtx->duration / 10; 
            // Seek to a keyframe backwards from the target timestamp
            av_seek_frame(fmtCtx, -1, targetTimestamp, AVSEEK_FLAG_BACKWARD);
        }

        bool frameDecoded = false;
        int maxAttempts = 50; // Don't loop infinitely if video is corrupted
        int attempts = 0;

        while (av_read_frame(fmtCtx, packet) >= 0 && attempts < maxAttempts) {
            if (packet->stream_index == videoStreamIndex) {
                // Send packet to decoder
                int sendRet = avcodec_send_packet(codecCtx, packet);
                if (sendRet == 0 || sendRet == AVERROR(EAGAIN)) {
                    // Receive frame
                    int recvRet = avcodec_receive_frame(codecCtx, frame);
                    if (recvRet == 0) {
                        frameDecoded = true;
                        break;
                    } else if (recvRet == AVERROR(EAGAIN)) {
                        // Needs more data
                    } else if (recvRet == AVERROR_EOF) {
                        break;
                    } else if (recvRet < 0) {
                        qWarning() << "Error during decoding.";
                        break;
                    }
                }
            }
            av_packet_unref(packet);
            attempts++;
        }

        // If a frame was decoded, convert it to QImage (RGB32)
        if (frameDecoded) {
            int w = frame->width;
            int h = frame->height;
            
            // Choose scaling flags - BILINEAR is usually fast and good enough for thumbnails
            swsCtx = sws_getContext(w, h, codecCtx->pix_fmt,
                                    w, h, AV_PIX_FMT_RGB32,
                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

            if (swsCtx) {
                // Pre-allocate completely opaque buffer
                QImage img(w, h, QImage::Format_RGB32);
                img.fill(Qt::black);
                
                uint8_t* dst[4] = { img.bits(), nullptr, nullptr, nullptr };
                int dstLinesize[4] = { static_cast<int>(img.bytesPerLine()), 0, 0, 0 };

                sws_scale(swsCtx, frame->data, frame->linesize, 0, h, dst, dstLinesize);

                result.image = img.copy(); // Deep copy so QImage owns the data
                result.success = true;
                result.message = ThumbnailExtractorMessage::Success;
            }
        } else {
            result.message = ThumbnailExtractorMessage::DecodeError;
            qWarning() << "FFmpegThumbnailExtractor: Failed to decode any frames.";
        }
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