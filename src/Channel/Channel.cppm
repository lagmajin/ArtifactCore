module;
#include <utility>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <map>

module Channel;

namespace ArtifactCore {

// --- VideoChannel Implementation ---

VideoChannel::VideoChannel(int width, int height) : width_(width), height_(height) {
    data_.resize(static_cast<size_t>(width) * height, 0.0f);
}

VideoChannel::~VideoChannel() = default;

void VideoChannel::resize(int width, int height) {
    width_ = width;
    height_ = height;
    data_.resize(static_cast<size_t>(width) * height);
}

void VideoChannel::clear(float value) {
    std::fill(data_.begin(), data_.end(), value);
}

// --- VideoFrame Implementation ---

VideoFrame::VideoFrame(int width, int height) : width_(width), height_(height) {
    // デフォルトでRGBAを追加
    addChannel(ChannelType::Red);
    addChannel(ChannelType::Green);
    addChannel(ChannelType::Blue);
    addChannel(ChannelType::Alpha);
}

VideoFrame::~VideoFrame() = default;

void VideoFrame::addChannel(ChannelType type, const std::string& name) {
    if (channels_.find(type) == channels_.end()) {
        channels_[type] = std::make_shared<VideoChannel>(width_, height_);
    }
}

void VideoFrame::removeChannel(ChannelType type) {
    channels_.erase(type);
}

bool VideoFrame::hasChannel(ChannelType type) const {
    return channels_.find(type) != channels_.end();
}

std::shared_ptr<VideoChannel> VideoFrame::getChannel(ChannelType type) {
    auto it = channels_.find(type);
    if (it != channels_.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace ArtifactCore
