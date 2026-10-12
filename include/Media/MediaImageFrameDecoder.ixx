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
    bool d3d11vaRequested_ = false;
    bool d3d12vaRequested_ = false;
    int64_t lastPts_ = 0;
    int displayRotationDegrees_ = 0;

public:
    MediaImageFrameDecoder();
    ~MediaImageFrameDecoder();

    bool initialize(AVCodecParameters* codecParams, int displayRotationDegrees = 0,
                    bool enableD3D11va = false, bool enableVulkan = false,
                    bool enableD3D12va = false,
                    void* sharedD3D12Device = nullptr);
    void setVulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex);
    QImage decodeFrame(AVPacket* packet);
    DecodedVideoFrame decodeFrameRaw(AVPacket* packet);
    
    // New methods for robust decoding
    int sendPacket(AVPacket* packet);
    QImage receiveFrame();
    DecodedVideoFrame receiveFrameRaw();
    DecodedVideoFrame receiveFrameRaw(bool retainD3D12Frame);
    
    void flush();
    int64_t getLastDecodedPts() const { return lastPts_; }
    
    AVCodecContext* getCodecContext() const { return codecContext_; }
    SwsContext* getSwsContext() const { return swsCtx_; }
    AVBufferRef* getHwDeviceContext() const { return hwDeviceCtx_; }
};

} // namespace ArtifactCore
