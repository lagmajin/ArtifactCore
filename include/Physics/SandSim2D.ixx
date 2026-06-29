module;
#include "../Define/DllExportMacro.hpp"
#include <vector>
#include <cstdint>
#include <random>

export module Physics.SandSim2D;

export namespace ArtifactCore {

enum class SandMaterial : uint8_t {
    Empty = 0,
    Sand,
    Water,
    Stone,
    Wood,
    Fire,
    Smoke,
    Acid
};

class LIBRARY_DLL_API SandSim2D {
public:
    SandSim2D(int width, int height);
    ~SandSim2D();

    void update(int substeps = 1);

    void setCell(int x, int y, SandMaterial m);
    SandMaterial getCell(int x, int y) const;

    void fillRect(int x, int y, int w, int h, SandMaterial m);
    void fillCircle(int cx, int cy, int radius, SandMaterial m);
    void clear();

    int width() const { return width_; }
    int height() const { return height_; }
    int gridSize() const { return size_; }

    const std::vector<SandMaterial>& grid() const { return grid_; }
    const std::vector<uint8_t>& lifetime() const { return lifetime_; }

    void setRandomSeed(unsigned seed);

private:
    int width_;
    int height_;
    int size_;

    std::vector<SandMaterial> grid_;
    std::vector<SandMaterial> next_;
    std::vector<uint8_t> lifetime_;
    std::vector<bool> updated_;

    std::mt19937 rng_;

    inline int IX(int x, int y) const {
        return x + y * width_;
    }

    bool inBounds(int x, int y) const;
    bool isEmpty(int x, int y) const;
    bool isSwappable(int x, int y) const;
    void swapCells(int x1, int y1, int x2, int y2);
    void updateSand(int x, int y, SandMaterial mat);
    void updateWater(int x, int y);
    void updateFire(int x, int y);
    void updateSmoke(int x, int y);
    void updateAcid(int x, int y);
    void igniteWood(int x, int y);
};

} // namespace ArtifactCore
