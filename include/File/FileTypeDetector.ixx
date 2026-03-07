module;
#include <QString>
#include <QByteArray>

export module File.TypeDetector;

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





export namespace ArtifactCore {

 enum class FileType {
  Unknown,
  Image,
  Video,
  Audio,
  Text,
  Binary,
  Document,
  Archive,
  Model3D
 };

 class FileTypeDetector {
 private:
  class Impl;
  Impl* impl_;
 public:
  FileTypeDetector();
  ~FileTypeDetector();

  FileType detect(const QString& filePath) const;
  FileType detectByExtension(const QString& filePath) const;
  FileType detectByMagicNumber(const QString& filePath) const;
 };

}