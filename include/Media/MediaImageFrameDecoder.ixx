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
    int64_t lastPts_ = 0;

public:
    MediaImageFrameDecoder();
    ~MediaImageFrameDecoder();

    bool initialize(AVCodecParameters* codecParams);
    QImage decodeFrame(AVPacket* packet);
    
    // New methods for robust decoding
    int sendPacket(AVPacket* packet);
    QImage receiveFrame();
    
    void flush();
    int64_t getLastDecodedPts() const { return lastPts_; }
    
    AVCodecContext* getCodecContext() const { return codecContext_; }
    SwsContext* getSwsContext() const { return swsCtx_; }
};

} // namespace ArtifactCore