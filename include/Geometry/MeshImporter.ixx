module;
//#include <assimp/>
#include <memory>
export module MeshImporter;

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



import Mesh;
import Utils.String.UniString; // KvɉėL

export namespace ArtifactCore {

class MeshImporter {
private:
    class Impl;
    Impl* impl_;
public:
    MeshImporter();
    ~MeshImporter();
    // t@CMesh𐶐
    std::shared_ptr<Mesh> importMeshFromFile(const UniString& path);
};

}