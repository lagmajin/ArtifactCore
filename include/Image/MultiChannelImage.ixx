module;
#include "../Define/DllExportMacro.hpp"
#include <cstddef>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module Image.MultiChannelImage;

export import Channel;

export namespace ArtifactCore
{

class LIBRARY_DLL_API MultiChannelImage
{
public:
  using ChannelMap = std::map<ChannelType, std::shared_ptr<VideoChannel>>;

  MultiChannelImage();
  MultiChannelImage(int width, int height);
  MultiChannelImage(const MultiChannelImage& other);
  MultiChannelImage& operator=(const MultiChannelImage& other);
  MultiChannelImage(MultiChannelImage&&) noexcept = default;
  MultiChannelImage& operator=(MultiChannelImage&&) noexcept = default;
  ~MultiChannelImage();

  void resize(int width, int height);
  void clear(float value = 0.0f);
  void addChannel(ChannelType type, const std::string& name = "");
  void removeChannel(ChannelType type);
  bool hasChannel(ChannelType type) const;
  void clearChannel(ChannelType type, float value = 0.0f);

  std::shared_ptr<VideoChannel> getChannel(ChannelType type);
  std::shared_ptr<const VideoChannel> getChannel(ChannelType type) const;

  int width() const;
  int height() const;
  bool isEmpty() const;
  std::size_t channelCount() const;

  std::vector<ChannelType> channelTypes() const;

  VideoFrame toVideoFrame() const;
  void copyFrom(const VideoFrame& frame);

private:
  void ensureBaseChannels();

  int width_ = 0;
  int height_ = 0;
  ChannelMap channels_;
};

} // namespace ArtifactCore

namespace ArtifactCore
{

inline MultiChannelImage::MultiChannelImage()
{
  ensureBaseChannels();
}

inline MultiChannelImage::MultiChannelImage(int width, int height)
  : width_(width), height_(height)
{
  ensureBaseChannels();
  resize(width, height);
}

inline MultiChannelImage::MultiChannelImage(const MultiChannelImage& other)
  : width_(other.width_), height_(other.height_), channels_(other.channels_)
{
}

inline MultiChannelImage& MultiChannelImage::operator=(const MultiChannelImage& other)
{
  if (this != &other) {
    width_ = other.width_;
    height_ = other.height_;
    channels_ = other.channels_;
  }
  return *this;
}

inline MultiChannelImage::~MultiChannelImage() = default;

inline void MultiChannelImage::ensureBaseChannels()
{
  addChannel(ChannelType::Red);
  addChannel(ChannelType::Green);
  addChannel(ChannelType::Blue);
  addChannel(ChannelType::Alpha);
}

inline void MultiChannelImage::resize(int width, int height)
{
  width_ = width;
  height_ = height;
  for (auto& [type, channel] : channels_) {
    if (channel) {
      channel->resize(width_, height_);
    }
  }
}

inline void MultiChannelImage::clear(float value)
{
  for (auto& [type, channel] : channels_) {
    if (channel) {
      channel->clear(value);
    }
  }
}

inline void MultiChannelImage::addChannel(ChannelType type, const std::string&)
{
  auto it = channels_.find(type);
  if (it == channels_.end()) {
    channels_.emplace(type, std::make_shared<VideoChannel>(width_, height_));
  }
}

inline void MultiChannelImage::removeChannel(ChannelType type)
{
  channels_.erase(type);
}

inline bool MultiChannelImage::hasChannel(ChannelType type) const
{
  return channels_.find(type) != channels_.end();
}

inline void MultiChannelImage::clearChannel(ChannelType type, float value)
{
  auto channel = getChannel(type);
  if (channel) {
    channel->clear(value);
  }
}

inline std::shared_ptr<VideoChannel> MultiChannelImage::getChannel(ChannelType type)
{
  auto it = channels_.find(type);
  if (it != channels_.end()) {
    return it->second;
  }
  return nullptr;
}

inline std::shared_ptr<const VideoChannel> MultiChannelImage::getChannel(ChannelType type) const
{
  auto it = channels_.find(type);
  if (it != channels_.end()) {
    return it->second;
  }
  return nullptr;
}

inline int MultiChannelImage::width() const
{
  return width_;
}

inline int MultiChannelImage::height() const
{
  return height_;
}

inline bool MultiChannelImage::isEmpty() const
{
  return width_ <= 0 || height_ <= 0 || channels_.empty();
}

inline std::size_t MultiChannelImage::channelCount() const
{
  return channels_.size();
}

inline std::vector<ChannelType> MultiChannelImage::channelTypes() const
{
  std::vector<ChannelType> types;
  types.reserve(channels_.size());
  for (const auto& [type, channel] : channels_) {
    if (channel) {
      types.push_back(type);
    }
  }
  return types;
}

inline VideoFrame MultiChannelImage::toVideoFrame() const
{
  VideoFrame frame(width_, height_);
  for (const auto& [type, channel] : channels_) {
    auto dst = frame.getChannel(type);
    if (!dst) {
      frame.addChannel(type);
      dst = frame.getChannel(type);
    }
    if (channel && dst) {
      const std::size_t count = std::min(channel->size(), dst->size());
      std::copy(channel->data(), channel->data() + count, dst->data());
    }
  }
  return frame;
}

inline void MultiChannelImage::copyFrom(const VideoFrame& frame)
{
  width_ = frame.width();
  height_ = frame.height();
  channels_.clear();
  ensureBaseChannels();

  auto copyIfPresent = [&](ChannelType type) {
    auto src = frame.getChannel(type);
    if (!src) {
      return;
    }
    auto dst = getChannel(type);
    if (!dst) {
      addChannel(type);
      dst = getChannel(type);
    }
    if (dst) {
      dst->resize(width_, height_);
      const std::size_t count = std::min(src->size(), dst->size());
      std::copy(src->data(), src->data() + count, dst->data());
    }
  };

  copyIfPresent(ChannelType::Red);
  copyIfPresent(ChannelType::Green);
  copyIfPresent(ChannelType::Blue);
  copyIfPresent(ChannelType::Alpha);
  copyIfPresent(ChannelType::Depth);
  copyIfPresent(ChannelType::NormalX);
  copyIfPresent(ChannelType::NormalY);
  copyIfPresent(ChannelType::NormalZ);
  copyIfPresent(ChannelType::VelocityX);
  copyIfPresent(ChannelType::VelocityY);
  copyIfPresent(ChannelType::ObjectId);
  copyIfPresent(ChannelType::MaterialId);
  copyIfPresent(ChannelType::Emission);
  copyIfPresent(ChannelType::Custom);
}

} // namespace ArtifactCore
