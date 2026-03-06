module;
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

export module Math.SpatialGrid;

import Particle;

namespace ArtifactCore {

/**
 * @brief 3D Spatial Grid for fast neighbor lookups (O(N) instead of O(N^2))
 */
export class SpatialGrid {
public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void clear() {
        grid_.clear();
    }

    /**
     * @brief Builds the grid from a list of positions
     */
    void build(const std::vector<float3>& positions) {
        grid_.clear();
        for (size_t i = 0; i < positions.size(); ++i) {
            uint64_t key = getHash(positions[i]);
            grid_.push_back({key, i});
        }
        // Sort by cell hash to group particles in the same cell
        std::sort(grid_.begin(), grid_.end(), [](const auto& a, const auto& b) {
            return a.cellKey < b.cellKey;
        });
    }

    /**
     * @brief Finds neighbors within the search radius
     * @param position Search center
     * @param radius Search radius
     * @param callback Function called for each neighbor index found
     */
    void query(const float3& position, float radius, std::function<void(size_t)> callback) const {
        if (grid_.empty()) return;

        int minX = static_cast<int>(std::floor((position.x - radius) / cellSize_));
        int maxX = static_cast<int>(std::floor((position.x + radius) / cellSize_));
        int minY = static_cast<int>(std::floor((position.y - radius) / cellSize_));
        int maxY = static_cast<int>(std::floor((position.y + radius) / cellSize_));
        int minZ = static_cast<int>(std::floor((position.z - radius) / cellSize_));
        int maxZ = static_cast<int>(std::floor((position.z + radius) / cellSize_));

        float radiusSq = radius * radius;

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    uint64_t key = hashCoords(x, y, z);
                    
                    // Binary search for the first element in this cell
                    auto it = std::lower_bound(grid_.begin(), grid_.end(), key, 
                        [](const auto& pair, uint64_t k) { return pair.cellKey < k; });
                    
                    while (it != grid_.end() && it->cellKey == key) {
                        callback(it->particleIndex);
                        ++it;
                    }
                }
            }
        }
    }

private:
    uint64_t getHash(const float3& pos) const {
        return hashCoords(
            static_cast<int>(std::floor(pos.x / cellSize_)),
            static_cast<int>(std::floor(pos.y / cellSize_)),
            static_cast<int>(std::floor(pos.z / cellSize_))
        );
    }

    uint64_t hashCoords(int x, int y, int z) const {
        // Simple 3D spatial hash
        const uint64_t p1 = 73856093;
        const uint64_t p2 = 19349663;
        const uint64_t p3 = 83492791;
        return (static_cast<uint64_t>(x) * p1) ^ 
               (static_cast<uint64_t>(y) * p2) ^ 
               (static_cast<uint64_t>(z) * p3);
    }

    struct Entry {
        uint64_t cellKey;
        size_t particleIndex;
    };

    float cellSize_;
    std::vector<Entry> grid_;
};

} // namespace ArtifactCore
