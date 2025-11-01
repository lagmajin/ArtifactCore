module;
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>

}

module Media.Encoder.FFMpegAudioEncoder;


namespace ArtifactCore
{
 class FFMpegAudioEncoder::Impl
 {
 public:
  Impl();
  ~Impl();
  void openEncoder();
  void closeEncoder();
  AVFormatContext* fmt_ctx = nullptr;   // 出力コンテキスト
  AVCodecContext* codec_ctx = nullptr; // コーデックコンテキスト
  AVStream* stream = nullptr;

  // --- Audio Format Info ---
  AVSampleFormat   sample_fmt = AV_SAMPLE_FMT_FLTP;
  int              sample_rate = 48000;
  int              channels = 2;

  // --- Buffers ---
  AVFrame* frame = nullptr;     // エンコード入力用フレーム
  AVPacket* pkt = nullptr;
 };

 void FFMpegAudioEncoder::Impl::openEncoder()
 {

 }

 void FFMpegAudioEncoder::Impl::closeEncoder()
 {

 }

 FFMpegAudioEncoder::Impl::Impl()
 {

 }

 FFMpegAudioEncoder::Impl::~Impl()
 {

 }

 FFMpegAudioEncoder::FFMpegAudioEncoder() : impl_(new Impl())
 {

 }

 FFMpegAudioEncoder::~FFMpegAudioEncoder()
 {
  delete impl_;
 }

}
