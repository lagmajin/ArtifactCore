module;

#include "../Define/DllExportMacro.hpp"
#include <QString>

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
export module Utils.Fingerprint;





export namespace ArtifactCore {

class LIBRARY_DLL_API AssetFingerprint {
public:
    // ファイルの内容からハッシュ（指紋）を生成
    static QString calculateMd5(const QString& filePath);
    static QString calculateSha256(const QString& filePath);

    // 大容量ファイル用に、先頭・中間・末尾の特定のブロックだけをハッシュ化する高速版
    static QString calculateFastHash(const QString& filePath);
};

} // namespace ArtifactCore
