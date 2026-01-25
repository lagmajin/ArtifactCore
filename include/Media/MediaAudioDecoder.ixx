module;
#define QT_NO_KEYWORDS
#include <QByteArray>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
export module MediaAudioDecoder;



export namespace ArtifactCore {

class MediaAudioDecoder {
private:
    AVCodecContext* codecContext_ = nullptr;
    SwrContext* swrCtx_ = nullptr;

public:
    MediaAudioDecoder();
    ~MediaAudioDecoder();

    bool initialize(AVCodecParameters* codecParams);
    QByteArray decodeFrame(AVPacket* packet);
    void flush();
};

} // namespace ArtifactCore