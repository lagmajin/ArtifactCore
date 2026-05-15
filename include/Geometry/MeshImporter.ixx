module;
//#include <assimp/>
#include <memory>
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>

export module MeshImporter;

import Mesh;
import Utils.String.UniString; // KvɉėL

export namespace ArtifactCore {

class MeshImporter {
public:
    enum class Backend {
        None,
        Ufbx,
        TinyObj,
        UfbxGltf,
        PMD
    };
private:
    class Impl;
    Impl* impl_;
public:
    MeshImporter();
    ~MeshImporter();

    MeshImporter(const MeshImporter&) = delete;
    MeshImporter& operator=(const MeshImporter&) = delete;

    // t@CMesh𐶐
    std::shared_ptr<Mesh> importMeshFromFile(const UniString& path);
    [[nodiscard]] Backend lastBackend() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString lastBaseColorTexture() const;
    [[nodiscard]] QString lastMetallicRoughnessTexture() const;
    [[nodiscard]] QString lastNormalTexture() const;
    [[nodiscard]] QString lastEmissionTexture() const;
    [[nodiscard]] QString lastOcclusionTexture() const;
    [[nodiscard]] QString lastOpacityTexture() const;
};

}
