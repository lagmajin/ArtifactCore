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
import Memory.SharedPtr;
import Utils.String.UniString; // KvɉėL

export namespace ArtifactCore {

class MeshImporter {
public:
    enum class Backend {
        None,
        Ufbx,
        TinyObj,
        UfbxGltf,
        PMD,
        Usda,
        Usdc,
        Usdz,
        Stl,
        Ply,
        Las
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
    SharedPtr<Mesh> importMeshFromFile(const UniString& path);
    SharedPtr<Mesh> importMeshFromFileAtTime(const UniString& path,
                                             double time,
                                             int clipIndex = 0);
    // Re-evaluate only the imported skin pose using the source scene retained
    // by this importer. The mesh topology and bind-space buffers stay intact.
    bool updateSkinPose(const UniString& path, double time, int clipIndex,
                        Mesh& mesh);
    [[nodiscard]] Backend lastBackend() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString lastBaseColorTexture() const;
    [[nodiscard]] QString lastMetallicRoughnessTexture() const;
    [[nodiscard]] QString lastNormalTexture() const;
    [[nodiscard]] QString lastEmissionTexture() const;
    [[nodiscard]] QString lastOcclusionTexture() const;
    [[nodiscard]] QString lastOpacityTexture() const;
    // PBR scalar factors from the first ufbx material that specifies them
    // (glTF metallic/roughness factors, FBX equivalents). Colorspace-free
    // scalars only; base-color factors stay out (sRGB/linear ambiguity).
    [[nodiscard]] bool hasLastMetallicFactor() const;
    [[nodiscard]] float lastMetallicFactor() const;
    [[nodiscard]] bool hasLastRoughnessFactor() const;
    [[nodiscard]] float lastRoughnessFactor() const;

    // Per-source-mesh data for importers that concatenate several primitives
    // into one mesh (a USD stage with multiple UsdGeomMesh prims).  Each entry
    // lines up with one Mesh::MaterialSlot range, in the same order.  Both
    // containers are empty for the ordinary single-mesh import path, and
    // lastXxx()/lastXxxFactor() keep describing the first entry in that case.
    struct SourceMeshTransform {
        float transform[16] = {};  // row-major, ready for InstanceData
    };
    [[nodiscard]] int sourceMeshCount() const;
    [[nodiscard]] const std::vector<SourceMeshTransform>& sourceMeshTransforms() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshBaseColorTextures() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshMetallicRoughnessTextures() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshNormalTextures() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshEmissionTextures() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshOcclusionTextures() const;
    [[nodiscard]] const std::vector<UniString>& sourceMeshOpacityTextures() const;
    [[nodiscard]] const std::vector<float>& sourceMeshMetallicFactors() const;
    [[nodiscard]] const std::vector<float>& sourceMeshRoughnessFactors() const;
    [[nodiscard]] bool hasSourceMeshMetallicFactor(int index) const;
    [[nodiscard]] bool hasSourceMeshRoughnessFactor(int index) const;

    // Identity export.  After a USD file is imported, the opened stage stays
    // with this importer so the source composition can be written back out as
    // is - hierarchy, material bindings, skeletons and blend shapes included -
    // instead of being rebuilt from the flattened Mesh.  The retained stage is
    // dropped as soon as a non-USD file is imported.
    [[nodiscard]] bool hasLoadedUsdStage() const;
    [[nodiscard]] QString loadedUsdPath() const;
    // Writes the retained stage to `outputPath`.  The destination extension
    // selects the file format (.usda / .usdc / .usdz).  Returns false and
    // records the reason in lastError() when no stage is held or the write
    // fails.
    bool exportLoadedUsdStage(const UniString& outputPath);
};

}
