module;
#include <utility>
#include <QString>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
}
extern "C" {
#include <vulkan/vulkan.h>
}
export module MediaImageFrameDecoder;

import Video.VideoFrame;



export namespace ArtifactCore {

class MediaImageFrameDecoder {
private:
    AVCodecContext* codecContext_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    AVBufferRef* frameCtx_ = nullptr;
    int64_t lastPts_ = 0;

public:
    MediaImageFrameDecoder();
    ~MediaImageFrameDecoder();

    bool initialize(AVCodecParameters* codecParams);
    void setVulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex);
    QImage decodeFrame(AVPacket* packet);
    DecodedVideoFrame decodeFrameRaw(AVPacket* packet);
    
    // New methods for robust decoding
    int sendPacket(AVPacket* packet);
    QImage receiveFrame();
    DecodedVideoFrame receiveFrameRaw();
    
    void flush();
    int64_t getLastDecodedPts() const { return lastPts_; }
    
    AVCodecContext* getCodecContext() const { return codecContext_; }
    SwsContext* getSwsContext() const { return swsCtx_; }
    AVBufferRef* getHwDeviceContext() const { return hwDeviceCtx_; }
};

} // namespace ArtifactCore
