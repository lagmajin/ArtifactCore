module;
#include <QString>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
export module MediaImageFrameDecoder;



export namespace ArtifactCore {

class MediaImageFrameDecoder {
private:
    AVCodecContext* codecContext_ = nullptr;
    SwsContext* swsCtx_ = nullptr;

public:
    MediaImageFrameDecoder();
    ~MediaImageFrameDecoder();

    bool initialize(AVCodecParameters* codecParams);
    QImage decodeFrame(AVPacket* packet);
    void flush();
};

} // namespace ArtifactCore