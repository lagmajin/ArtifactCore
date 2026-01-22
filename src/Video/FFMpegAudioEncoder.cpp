module;
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>

}

module Media.Encoder.FFmpegAudioEncoder;


namespace ArtifactCore
{
 class FFmpegAudioEncoder::Impl
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

 void FFmpegAudioEncoder::Impl::openEncoder()
 {

 }

 void FFmpegAudioEncoder::Impl::closeEncoder()
 {

 }

 FFmpegAudioEncoder::Impl::Impl()
 {

 }

 FFmpegAudioEncoder::Impl::~Impl()
 {

 }

 FFmpegAudioEncoder::FFmpegAudioEncoder() : impl_(new Impl())
 {

 }

 FFmpegAudioEncoder::~FFmpegAudioEncoder()
 {
  delete impl_;
 }

}
