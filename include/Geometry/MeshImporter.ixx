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
    [[nodiscard]] QString lastClearcoatRoughnessTexture() const;
    // PBR scalar factors from the first ufbx material that specifies them
    // (glTF metallic/roughness factors, FBX equivalents). Colorspace-free
    // scalars only; base-color factors stay out (sRGB/linear ambiguity).
    [[nodiscard]] bool hasLastMetallicFactor() const;
    [[nodiscard]] float lastMetallicFactor() const;
    [[nodiscard]] bool hasLastRoughnessFactor() const;
    [[nodiscard]] float lastRoughnessFactor() const;
    [[nodiscard]] bool hasLastClearcoatFactor() const;
    [[nodiscard]] float lastClearcoatFactor() const;
    [[nodiscard]] bool hasLastClearcoatRoughnessFactor() const;
    [[nodiscard]] float lastClearcoatRoughnessFactor() const;
    [[nodiscard]] bool hasLastTransmissionFactor() const;
    [[nodiscard]] float lastTransmissionFactor() const;
    [[nodiscard]] bool hasLastIorFactor() const;
    [[nodiscard]] float lastIorFactor() const;
    [[nodiscard]] bool hasLastSheenFactor() const;
    [[nodiscard]] float lastSheenFactor() const;
    [[nodiscard]] bool hasLastSheenRoughnessFactor() const;
    [[nodiscard]] float lastSheenRoughnessFactor() const;

    // PBR values and texture paths indexed by the ufbx material table. Mesh
    // material slots refer to these indices after import.
    struct ImportedPbrTextureTransform {
        float scaleU = 1.0f;
        float scaleV = 1.0f;
        float rotationDegrees = 0.0f;
        float offsetU = 0.0f;
        float offsetV = 0.0f;
        int uvSet = 0;
        bool hasTransform = false;
    };
    struct ImportedPbrMaterial {
        UniString name;
        UniString baseColorTexture;
        UniString metallicRoughnessTexture;
        UniString metallicTexture;
        UniString roughnessTexture;
        UniString normalTexture;
        UniString clearcoatTexture;
        UniString clearcoatRoughnessTexture;
        UniString clearcoatNormalTexture;
        UniString emissionTexture;
        UniString occlusionTexture;
        UniString opacityTexture;
        ImportedPbrTextureTransform baseColorUv;
        ImportedPbrTextureTransform metallicRoughnessUv;
        ImportedPbrTextureTransform metallicUv;
        ImportedPbrTextureTransform roughnessUv;
        ImportedPbrTextureTransform normalUv;
        ImportedPbrTextureTransform clearcoatUv;
        ImportedPbrTextureTransform clearcoatRoughnessUv;
        ImportedPbrTextureTransform clearcoatNormalUv;
        ImportedPbrTextureTransform emissionUv;
        ImportedPbrTextureTransform occlusionUv;
        ImportedPbrTextureTransform opacityUv;
        float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float specularTint[3] = {1.0f, 1.0f, 1.0f};
        float sheenTint[3] = {1.0f, 1.0f, 1.0f};
        bool hasBaseColor = false;
        bool hasMetallic = false;
        bool hasRoughness = false;
        bool hasDiffuseRoughness = false;
        bool hasClearcoat = false;
        bool hasClearcoatRoughness = false;
        bool hasClearcoatIor = false;
        bool hasClearcoatTint = false;
        bool hasThinFilmThickness = false;
        bool hasThinFilmIor = false;
        bool hasTransmission = false;
        bool hasIor = false;
        bool hasSheen = false;
        bool hasSheenRoughness = false;
        bool hasSpecular = false;
        bool hasSpecularTint = false;
        bool hasSheenTint = false;
        bool hasSubsurface = false;
        bool hasSubsurfaceColor = false;
        bool hasSubsurfaceRadius = false;
        bool hasSubsurfaceAnisotropy = false;
        bool hasAnisotropy = false;
        bool hasAnisotropyRotation = false;
        bool hasOpacity = false;
        bool hasEmissionColor = false;
        bool hasEmissionStrength = false;
        float metallic = 0.0f;
        float roughness = 0.5f;
        float diffuseRoughness = 0.0f;
        float clearcoat = 0.0f;
        float clearcoatRoughness = 0.03f;
        float clearcoatIor = 1.5f;
        float clearcoatTint[3] = {1.0f, 1.0f, 1.0f};
        float thinFilmThickness = 0.0f;
        float thinFilmIor = 1.33f;
        float transmission = 0.0f;
        float ior = 1.5f;
        float sheen = 0.0f;
        float sheenRoughness = 0.5f;
        float specular = 0.5f;
        float subsurface = 0.0f;
        float subsurfaceAnisotropy = 0.0f;
        float anisotropy = 0.0f;
        float anisotropyRotationDegrees = 0.0f;
        float opacity = 1.0f;
        float emissionStrength = 0.0f;
        float subsurfaceColor[3] = {1.0f, 1.0f, 1.0f};
        float subsurfaceRadius[3] = {1.0f, 0.2f, 0.1f};
        float emissionColor[3] = {0.0f, 0.0f, 0.0f};
    };
    [[nodiscard]] const std::vector<ImportedPbrMaterial>&
    importedPbrMaterials() const;

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
    [[nodiscard]] const std::vector<UniString>& sourceMeshClearcoatRoughnessTextures() const;
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
