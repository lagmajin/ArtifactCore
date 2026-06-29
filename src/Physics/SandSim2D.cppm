module;
#include <algorithm>
#include <cmath>
#include <random>

module Physics.SandSim2D;

namespace ArtifactCore {

SandSim2D::SandSim2D(int width, int height)
    : width_(width), height_(height), size_(width * height)
    , grid_(size_, SandMaterial::Empty)
    , next_(size_, SandMaterial::Empty)
    , lifetime_(size_, 0)
    , updated_(size_, false)
    , rng_(std::random_device{}())
{
}

SandSim2D::~SandSim2D() = default;

void SandSim2D::setRandomSeed(unsigned seed) {
    rng_.seed(seed);
}

bool SandSim2D::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

bool SandSim2D::isEmpty(int x, int y) const {
    return inBounds(x, y) && grid_[IX(x, y)] == SandMaterial::Empty;
}

bool SandSim2D::isSwappable(int x, int y) const {
    if (!inBounds(x, y)) return false;
    auto m = grid_[IX(x, y)];
    return m == SandMaterial::Empty || m == SandMaterial::Water || m == SandMaterial::Smoke || m == SandMaterial::Acid;
}

void SandSim2D::swapCells(int x1, int y1, int x2, int y2) {
    int i1 = IX(x1, y1);
    int i2 = IX(x2, y2);
    std::swap(grid_[i1], grid_[i2]);
    std::swap(lifetime_[i1], lifetime_[i2]);
    updated_[i1] = true;
    updated_[i2] = true;
}

void SandSim2D::setCell(int x, int y, SandMaterial m) {
    if (inBounds(x, y)) {
        grid_[IX(x, y)] = m;
        if (m == SandMaterial::Fire) {
            lifetime_[IX(x, y)] = static_cast<uint8_t>(rng_() % 60 + 20);
        } else if (m == SandMaterial::Smoke) {
            lifetime_[IX(x, y)] = static_cast<uint8_t>(rng_() % 80 + 30);
        } else {
            lifetime_[IX(x, y)] = 0;
        }
    }
}

SandMaterial SandSim2D::getCell(int x, int y) const {
    if (inBounds(x, y)) {
        return grid_[IX(x, y)];
    }
    return SandMaterial::Empty;
}

void SandSim2D::fillRect(int x, int y, int w, int h, SandMaterial m) {
    for (int j = y; j < y + h; ++j) {
        for (int i = x; i < x + w; ++i) {
            setCell(i, j, m);
        }
    }
}

void SandSim2D::fillCircle(int cx, int cy, int radius, SandMaterial m) {
    int r2 = radius * radius;
    for (int j = cy - radius; j <= cy + radius; ++j) {
        for (int i = cx - radius; i <= cx + radius; ++i) {
            if (inBounds(i, j)) {
                int dx = i - cx;
                int dy = j - cy;
                if (dx * dx + dy * dy <= r2) {
                    setCell(i, j, m);
                }
            }
        }
    }
}

void SandSim2D::clear() {
    std::fill(grid_.begin(), grid_.end(), SandMaterial::Empty);
    std::fill(lifetime_.begin(), lifetime_.end(), 0);
    std::fill(updated_.begin(), updated_.end(), false);
}

void SandSim2D::igniteWood(int x, int y) {
    if (inBounds(x, y) && grid_[IX(x, y)] == SandMaterial::Wood) {
        grid_[IX(x, y)] = SandMaterial::Fire;
        lifetime_[IX(x, y)] = static_cast<uint8_t>(rng_() % 60 + 20);
        updated_[IX(x, y)] = true;
    }
}

void SandSim2D::updateSand(int x, int y, SandMaterial mat) {
    int idx = IX(x, y);

    // Try fall straight down
    if (isEmpty(x, y + 1)) {
        next_[IX(x, y + 1)] = mat;
        return;
    }
    // Swap with water below
    if (inBounds(x, y + 1) && grid_[IX(x, y + 1)] == SandMaterial::Water) {
        next_[IX(x, y + 1)] = mat;
        next_[idx] = SandMaterial::Water;
        return;
    }
    if (inBounds(x, y + 1) && grid_[IX(x, y + 1)] == SandMaterial::Acid) {
        next_[IX(x, y + 1)] = mat;
        next_[idx] = SandMaterial::Acid;
        return;
    }

    // Slide diagonally
    int dir = (rng_() % 2 == 0) ? -1 : 1;
    if (isEmpty(x + dir, y + 1)) {
        next_[IX(x + dir, y + 1)] = mat;
        return;
    }
    if (isEmpty(x - dir, y + 1)) {
        next_[IX(x - dir, y + 1)] = mat;
        return;
    }
    // Stay in place
    next_[idx] = mat;
}

void SandSim2D::updateWater(int x, int y) {
    int idx = IX(x, y);

    // Fall down
    if (isEmpty(x, y + 1)) {
        next_[IX(x, y + 1)] = SandMaterial::Water;
        return;
    }

    // Slide diagonally down
    int dir = (rng_() % 2 == 0) ? -1 : 1;
    if (isEmpty(x + dir, y + 1)) {
        next_[IX(x + dir, y + 1)] = SandMaterial::Water;
        return;
    }
    if (isEmpty(x - dir, y + 1)) {
        next_[IX(x - dir, y + 1)] = SandMaterial::Water;
        return;
    }

    // Spread horizontally
    if (isEmpty(x + dir, y)) {
        next_[IX(x + dir, y)] = SandMaterial::Water;
        return;
    }
    if (isEmpty(x - dir, y)) {
        next_[IX(x - dir, y)] = SandMaterial::Water;
        return;
    }

    next_[idx] = SandMaterial::Water;
}

void SandSim2D::updateFire(int x, int y) {
    int idx = IX(x, y);
    uint8_t& life = lifetime_[idx];

    if (life == 0) {
        next_[idx] = SandMaterial::Smoke;
        lifetime_[idx] = static_cast<uint8_t>(rng_() % 80 + 30);
        return;
    }
    --life;

    // Ignite adjacent wood
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (rng_() % 4 == 0) {
                igniteWood(x + dx, y + dy);
            }
        }
    }

    // Erratic upward movement
    int nx = x + (rng_() % 3 - 1);
    int ny = y - (rng_() % 2); // upward bias
    if (nx != x || ny != y) {
        if (inBounds(nx, ny) && isEmpty(nx, ny)) {
            next_[IX(nx, ny)] = SandMaterial::Fire;
            lifetime_[IX(nx, ny)] = life;
            next_[idx] = SandMaterial::Empty;
            return;
        }
    }

    next_[idx] = SandMaterial::Fire;
}

void SandSim2D::updateSmoke(int x, int y) {
    int idx = IX(x, y);
    uint8_t& life = lifetime_[idx];

    if (life == 0) {
        next_[idx] = SandMaterial::Empty;
        return;
    }
    --life;

    // Rise with random drift
    int nx = x + (rng_() % 3 - 1);
    int ny = y - 1;
    if (inBounds(nx, ny) && isEmpty(nx, ny)) {
        next_[IX(nx, ny)] = SandMaterial::Smoke;
        lifetime_[IX(nx, ny)] = life;
        return;
    }

    // Try staying in place with slight horizontal drift
    nx = x + (rng_() % 3 - 1);
    if (nx != x && inBounds(nx, y) && isEmpty(nx, y)) {
        next_[IX(nx, y)] = SandMaterial::Smoke;
        lifetime_[IX(nx, y)] = life;
        return;
    }

    next_[idx] = SandMaterial::Smoke;
}

void SandSim2D::updateAcid(int x, int y) {
    int idx = IX(x, y);

    // Dissolve adjacent non-stone materials
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int ax = x + dx, ay = y + dy;
            if (!inBounds(ax, ay)) continue;
            auto m = grid_[IX(ax, ay)];
            if (m != SandMaterial::Empty && m != SandMaterial::Stone && m != SandMaterial::Acid) {
                if (rng_() % 3 == 0) {
                    grid_[IX(ax, ay)] = SandMaterial::Empty;
                }
            }
        }
    }

    // Fall like water
    if (isEmpty(x, y + 1)) {
        next_[IX(x, y + 1)] = SandMaterial::Acid;
        return;
    }
    int dir = (rng_() % 2 == 0) ? -1 : 1;
    if (isEmpty(x + dir, y + 1)) {
        next_[IX(x + dir, y + 1)] = SandMaterial::Acid;
        return;
    }
    if (isEmpty(x - dir, y + 1)) {
        next_[IX(x - dir, y + 1)] = SandMaterial::Acid;
        return;
    }
    if (isEmpty(x + dir, y)) {
        next_[IX(x + dir, y)] = SandMaterial::Acid;
        return;
    }
    if (isEmpty(x - dir, y)) {
        next_[IX(x - dir, y)] = SandMaterial::Acid;
        return;
    }

    next_[idx] = SandMaterial::Acid;
}

void SandSim2D::update(int substeps) {
    for (int step = 0; step < substeps; ++step) {
        // Copy current to next (default: stay in place if not processed)
        next_ = grid_;
        std::fill(updated_.begin(), updated_.end(), false);

        // Process bottom-to-top for correct gravity
        for (int y = height_ - 1; y >= 0; --y) {
            for (int x = 0; x < width_; ++x) {
                int idx = IX(x, y);
                if (updated_[idx]) continue;

                switch (grid_[idx]) {
                case SandMaterial::Sand:
                    updateSand(x, y, SandMaterial::Sand);
                    break;
                case SandMaterial::Water:
                    updateWater(x, y);
                    break;
                case SandMaterial::Fire:
                    updateFire(x, y);
                    break;
                case SandMaterial::Smoke:
                    updateSmoke(x, y);
                    break;
                case SandMaterial::Acid:
                    updateAcid(x, y);
                    break;
                default:
                    break; // Stone, Wood, Empty stay in place
                }
            }
        }

        grid_.swap(next_);
    }
}

} // namespace ArtifactCore
