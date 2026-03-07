module;
#include <stb_image.h>
#include <stb_image_write.h>

#include <QVector>
export module IO.Image.Stb;

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

 struct WriteContext {
  QVector<unsigned char> buffer; // std::vector から QVector に変更
 };

 // この関数は stb_image_write から呼び出され、エンコードされたデータを受け取る
 void write_to_memory_callback(void* context, void* data, int size) {
  WriteContext* ctx = static_cast<WriteContext*>(context);
  const unsigned char* bytes = static_cast<const unsigned char*>(data); // const を追加


 }
 




};