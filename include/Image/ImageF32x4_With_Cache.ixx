module;
﻿
#include "../Define/DllExportMacro.hpp"
#include <QObject>
#include <wobjectdefs.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Image.ImageF32x4RGBAWithCache;




import Image.ImageF32x4_RGBA;

export namespace ArtifactCore
{
	class ImageF32x4_RGBA;


class LIBRARY_DLL_API ImageF32x4RGBAWithCache:public QObject{
W_OBJECT(ImageF32x4RGBAWithCache)
private:
class Impl;
Impl* impl_;
public:
ImageF32x4RGBAWithCache();
explicit ImageF32x4RGBAWithCache(const ImageF32x4_RGBA& image);
ImageF32x4RGBAWithCache(const ImageF32x4RGBAWithCache& other);
~ImageF32x4RGBAWithCache();
ImageF32x4_RGBA& image() const;

void UpdateGpuTextureFromCpuData();
void UpdateCpuDataFromGpuTexture();

int32_t width() const;
int32_t height() const;
bool IsGpuTextureValid() const;
  
// Deep copy methods
ImageF32x4RGBAWithCache DeepCopy() const;
ImageF32x4RGBAWithCache& operator=(const ImageF32x4RGBAWithCache& other);


 };

// Recommended: Use this typedef instead of std::shared_ptr<ImageF32x4RGBAWithCache>
// Example: ImageF32x4RGBAWithCachePtr myImage = std::make_shared<ImageF32x4RGBAWithCache>();
typedef std::shared_ptr<ImageF32x4RGBAWithCache> ImageF32x4RGBAWithCachePtr;














};