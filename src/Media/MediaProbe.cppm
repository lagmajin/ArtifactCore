module;
#include <utility>
#include <QtCore/QFile>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

module Media.MediaProbe;

import Media.MediaProbe;
import Media.Info;





namespace ArtifactCore {

 namespace {
  AVDictionary* makeSingleThreadStreamInfoOptions()
  {
   AVDictionary* opts = nullptr;
   av_dict_set(&opts, "threads", "1", 0);
   av_dict_set(&opts, "thread_type", "0", 0);
   return opts;
  }
 }







 class MediaProbePrivate {
 private:
  AVFormatContext* formatContext = nullptr;
  MediaInfo info_;
 public:
  MediaProbePrivate();
  ~MediaProbePrivate();
  void open(const QFile& file);
  void close();
  void getMetadata();
 };

 MediaProbePrivate::MediaProbePrivate()
 {

 }

 MediaProbePrivate::~MediaProbePrivate()
 {

 }

 void MediaProbePrivate::open(const QFile& file)
 {
  if (avformat_open_input(&formatContext, file.fileName().toUtf8().constData(), nullptr, nullptr) != 0) {
   //return false; // t@CðJ¯È¢ê
  }
  AVDictionary* streamInfoOpts = makeSingleThreadStreamInfoOptions();
  if (avformat_find_stream_info(formatContext, &streamInfoOpts) < 0) {
   av_dict_free(&streamInfoOpts);

  }
  av_dict_free(&streamInfoOpts);

  info_ = MediaInfo();

 }

 void MediaProbePrivate::close()
 {

 }

 void MediaProbePrivate::getMetadata()
 {

 }

 MediaProbe::MediaProbe()
 {

 }

 MediaProbe::~MediaProbe()
 {

 }

 void MediaProbe::open(const QFile& file)
 {

 }
}
