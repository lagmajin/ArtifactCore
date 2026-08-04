module;
#include <utility>
#include <vector>
#include <algorithm>
#include <map>
#include <limits>

module Channel;

namespace ArtifactCore {

// --- VideoChannel Implementation ---

VideoChannel::VideoChannel(int width, int height)
    : width_(std::max(0, width)), height_(std::max(0, height)) {
    data_.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0.0f);
}

VideoChannel::~VideoChannel() = default;

void VideoChannel::resize(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    data_.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_));
}

void VideoChannel::clear(float value) {
    std::fill(data_.begin(), data_.end(), value);
}

// --- VideoFrame Implementation ---

VideoFrame::VideoFrame(int width, int height)
    : width_(std::max(0, width)), height_(std::max(0, height)) {
    // デフォルトでRGBAを追加
    addChannel(ChannelType::Red);
    addChannel(ChannelType::Green);
    addChannel(ChannelType::Blue);
    addChannel(ChannelType::Alpha);
}

VideoFrame::~VideoFrame() = default;

void VideoFrame::addChannel(ChannelType type, const String& name) {
    if (channels_.find(type) == channels_.end()) {
        channels_[type] = makeShared<VideoChannel>(width_, height_);
    }
}

void VideoFrame::removeChannel(ChannelType type) {
    channels_.erase(type);
}

bool VideoFrame::hasChannel(ChannelType type) const {
    return channels_.find(type) != channels_.end();
}

SharedPtr<VideoChannel> VideoFrame::getChannel(ChannelType type) {
    auto it = channels_.find(type);
    if (it != channels_.end()) {
        return it->second;
    }
    return nullptr;
}

SharedPtr<const VideoChannel> VideoFrame::getChannel(ChannelType type) const {
    auto it = channels_.find(type);
    if (it != channels_.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace ArtifactCore
