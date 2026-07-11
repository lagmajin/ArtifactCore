module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
module Physics.Mpm2D;

namespace ArtifactCore {

// ---- static helpers ----

MpmMat2 MpmMat2::identity() noexcept { return {1,0,0,1}; }
MpmMat2 MpmMat2::zero()     noexcept { return {0,0,0,0}; }

// ---- B-spline weight and derivative (quadratic, 2-cell support) ----
// N(x)  domain: [-1.5, 1.5] in normalised coordinates r = x/dx

float MpmSolver2D::weight(float r, float /*dx*/) {
    float ra = std::abs(r);
    if (ra >= 1.5f) return 0.0f;
    if (ra >= 0.5f) return 0.5f * (1.5f - ra) * (1.5f - ra);
    return 0.75f - ra * ra;
}

float MpmSolver2D::dweight(float r, float dx) {
    float inv = 1.0f / dx;
    float ra  = std::abs(r);
    float sgn = (r > 0.0f) ? 1.0f : ((r < 0.0f) ? -1.0f : 0.0f);
    if (ra >= 1.5f) return 0.0f;
    if (ra >= 0.5f) return -(1.5f - ra) * sgn * inv;
    return -2.0f * r * inv;
}

// ---- 2x2 polar decomposition via Newton iteration ----

MpmMat2 MpmSolver2D::polarDecomposition(const MpmMat2& F) {
    MpmMat2 R = F;
    for (int iter = 0; iter < 20; ++iter) {
        float det = R.det();
        if (std::abs(det) < 1e-20f) break;
        float invDet = 1.0f / det;
        // (R^{-1})^T = adj(R)^T / det(R)
        // adj(R) = [[R11, -R01], [-R10, R00]]
        // adj(R)^T = [[R11, -R10], [-R01, R00]]
        MpmMat2 RTinv{ R.m11 * invDet, -R.m10 * invDet,
                       -R.m01 * invDet,  R.m00 * invDet };
        MpmMat2 Rnext;
        Rnext.m00 = 0.5f * (R.m00 + RTinv.m00);
        Rnext.m01 = 0.5f * (R.m01 + RTinv.m01);
        Rnext.m10 = 0.5f * (R.m10 + RTinv.m10);
        Rnext.m11 = 0.5f * (R.m11 + RTinv.m11);
        float diff = std::abs(Rnext.m00 - R.m00) + std::abs(Rnext.m01 - R.m01)
                   + std::abs(Rnext.m10 - R.m10) + std::abs(Rnext.m11 - R.m11);
        R = Rnext;
        if (diff < 1e-8f) break;
    }
    return R;
}

// ---- Fixed Corotated Elasticity : first Piola-Kirchhoff stress ----
// Psi(F) = mu * ||F - R||^2 + lambda/2 * (J-1)^2
// P = dPsi/dF = 2*mu*(F - R) + lambda*J*(J-1)*F^{-T}

MpmMat2 MpmSolver2D::firstPiolaKirchhoff(const MpmMat2& Fe) {
    float J = Fe.det();
    MpmMat2 R = polarDecomposition(Fe);
    MpmMat2 FinvT = Fe.inverse().transpose();

    MpmMat2 P;
    float twoMu = 2.0f * mu_;
    P.m00 = twoMu * (Fe.m00 - R.m00) + lambda_ * J * (J - 1.0f) * FinvT.m00;
    P.m01 = twoMu * (Fe.m01 - R.m01) + lambda_ * J * (J - 1.0f) * FinvT.m01;
    P.m10 = twoMu * (Fe.m10 - R.m10) + lambda_ * J * (J - 1.0f) * FinvT.m10;
    P.m11 = twoMu * (Fe.m11 - R.m11) + lambda_ * J * (J - 1.0f) * FinvT.m11;
    return P;
}

// ---- grid management ----

void MpmSolver2D::resizeGrid() {
    grid_.resize(nx_ * ny_);
}

void MpmSolver2D::resetGrid() {
    for (auto& node : grid_) {
        node.mass  = 0.0f;
        node.vel   = MpmVec2{};
        node.force = MpmVec2{};
    }
}

void MpmSolver2D::setGrid(float cellSize, int nx, int ny) {
    cellSize_ = std::max(0.1f, cellSize);
    nx_ = std::max(2, nx);
    ny_ = std::max(2, ny);
    resizeGrid();
}

void MpmSolver2D::setMaterial(float youngModulus, float poissonRatio) {
    E_  = std::max(1.0f, youngModulus);
    nu_ = std::clamp(poissonRatio, -0.99f, 0.49f);
    mu_     = E_ / (2.0f * (1.0f + nu_));
    lambda_ = E_ * nu_ / ((1.0f + nu_) * (1.0f - 2.0f * nu_));
}

void MpmSolver2D::setPlasticity(float yieldStress, float hardening) {
    yieldStress_ = std::max(0.0f, yieldStress);
    hardening_   = std::max(0.0f, hardening);
}

void MpmSolver2D::setFracture(float maxPlasticStrain, float fractureEnergy) {
    maxPlasticStrain_ = std::max(0.0f, maxPlasticStrain);
    fractureEnergy_   = std::max(0.0f, fractureEnergy);
}

void MpmSolver2D::applyMaterialPreset(MpmMaterialPreset preset) {
    switch (preset) {
    case MpmMaterialPreset::Flesh:
        setMaterial(2.5e4f, 0.47f);
        setPlasticity(1.2e3f, 0.03f);
        setFracture(2.0f, 0.0f);
        setDamping(0.08f);
        break;
    case MpmMaterialPreset::Foam:
        setMaterial(8.0e3f, 0.22f);
        setPlasticity(4.0e2f, 0.18f);
        setFracture(1.4f, 0.0f);
        setDamping(0.16f);
        break;
    case MpmMaterialPreset::HardRubber:
        setMaterial(1.8e5f, 0.46f);
        setPlasticity(5.0e4f, 0.0f);
        setFracture(5.0f, 0.0f);
        setDamping(0.025f);
        break;
    case MpmMaterialPreset::Wood:
        setMaterial(9.0e5f, 0.28f);
        setPlasticity(7.5e3f, 0.08f);
        setFracture(0.35f, 0.018f);
        setDamping(0.035f);
        break;
    }
}

void MpmSolver2D::setBoundary(float xmin, float ymin, float xmax, float ymax) {
    bxmin_ = xmin; bymin_ = ymin;
    bxmax_ = xmax; bymax_ = ymax;
    hasBoundary_ = true;
}

void MpmSolver2D::clear() {
    particles_.clear();
    fracturedIndices_.clear();
    colliders_.clear();
    particleGridColumns_ = 0;
    particleGridRows_ = 0;
}

int MpmSolver2D::activeCount() const noexcept {
    int n = 0;
    for (auto& p : particles_)
        if (p.active) ++n;
    return n;
}

void MpmSolver2D::clearFractureEvents() {
    fracturedIndices_.clear();
}

MpmSnapshot2D MpmSolver2D::snapshot() const {
    return {particles_, fracturedIndices_, gridOrigin_, accumulatedTime_,
            particleGridColumns_, particleGridRows_};
}

bool MpmSolver2D::canRestoreSnapshot(const MpmSnapshot2D& snapshot) const noexcept {
    if (snapshot.particleGridColumns < 0 || snapshot.particleGridRows < 0) {
        return false;
    }
    if (snapshot.particleGridColumns > 0 && snapshot.particleGridRows > 0 &&
        snapshot.particles.size() != static_cast<std::size_t>(
            snapshot.particleGridColumns * snapshot.particleGridRows)) {
        return false;
    }
    return std::isfinite(snapshot.gridOrigin.x) && std::isfinite(snapshot.gridOrigin.y) &&
           std::isfinite(snapshot.accumulatedTime);
}

bool MpmSolver2D::restoreSnapshot(const MpmSnapshot2D& snapshot) {
    if (!canRestoreSnapshot(snapshot)) return false;
    particles_ = snapshot.particles;
    fracturedIndices_ = snapshot.fracturedIndices;
    gridOrigin_ = snapshot.gridOrigin;
    accumulatedTime_ = std::clamp(snapshot.accumulatedTime, 0.0f,
                                  fixedDt_ * static_cast<float>(maxSubsteps_));
    particleGridColumns_ = snapshot.particleGridColumns;
    particleGridRows_ = snapshot.particleGridRows;
    return true;
}

// ---- particle generation ----

void MpmSolver2D::addParticlesGrid(float cx, float cy, float w, float h,
                                   int cols, int rows, float density) {
    int nc = std::max(1, cols);
    int nr = std::max(1, rows);
    particleGridColumns_ = nc;
    particleGridRows_ = nr;
    float stepX = (nc > 1) ? w / static_cast<float>(nc - 1) : 0.0f;
    float stepY = (nr > 1) ? h / static_cast<float>(nr - 1) : 0.0f;
    float startX = cx - w * 0.5f;
    float startY = cy - h * 0.5f;

    float totalVol = w * h * 1.0f; // unit thickness
    float particleVol = totalVol / static_cast<float>(nc * nr);
    float particleMass = density * particleVol;

    for (int r = 0; r < nr; ++r) {
        for (int c = 0; c < nc; ++c) {
            MpmParticle2D p;
            p.pos.x    = startX + static_cast<float>(c) * stepX;
            p.pos.y    = startY + static_cast<float>(r) * stepY;
            p.mass     = particleMass;
            p.volume0  = particleVol;
            p.vel      = MpmVec2{};
            p.F        = MpmMat2::identity();
            p.Fp       = MpmMat2::identity();
            p.C        = MpmMat2::zero();
            p.active   = true;
            p.plasticStrain = 0.0f;

            // colour proportional to position
            p.r = 0.3f + 0.5f * (p.pos.x - startX) / (w + 1.0f);
            p.g = 0.3f + 0.5f * (p.pos.y - startY) / (h + 1.0f);
            p.b = 0.2f;
            particles_.push_back(p);
        }
    }
}

void MpmSolver2D::addParticlesRandom(float cx, float cy, float radius,
                                     int count, float density) {
    int n = std::max(1, count);
    float area   = 3.14159f * radius * radius;
    float totalVol = area * 1.0f;
    float particleVol = totalVol / static_cast<float>(n);
    float particleMass = density * particleVol;

    for (int i = 0; i < n; ++i) {
        float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        float r = radius * std::sqrt(static_cast<float>(i) / static_cast<float>(n));
        MpmParticle2D p;
        p.pos.x    = cx + r * std::cos(angle);
        p.pos.y    = cy + r * std::sin(angle);
        p.mass     = particleMass;
        p.volume0  = particleVol;
        p.vel      = MpmVec2{};
        p.F        = MpmMat2::identity();
        p.Fp       = MpmMat2::identity();
        p.C        = MpmMat2::zero();
        p.active   = true;
        p.plasticStrain = 0.0f;
        p.r = 0.7f; p.g = 0.3f; p.b = 0.3f;
        particles_.push_back(p);
    }
}

// ---- MPM step: P2G ----

void MpmSolver2D::particleToGrid() {
    float dx  = cellSize_;
    float inv = 1.0f / dx;

    for (auto& p : particles_) {
        if (!p.active) continue;

        // cell index
        float fx = (p.pos.x - gridOrigin_.x) * inv;
        float fy = (p.pos.y - gridOrigin_.y) * inv;
        int ix = static_cast<int>(std::floor(fx));
        int iy = static_cast<int>(std::floor(fy));

        for (int dj = -1; dj <= 2; ++dj) {
            for (int di = -1; di <= 2; ++di) {
                int ni = ix + di;
                int nj = iy + dj;
                if (ni < 0 || ni >= nx_ || nj < 0 || nj >= ny_) continue;

                float rx = p.pos.x - (gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx);
                float ry = p.pos.y - (gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx);
                float w  = weight(rx, dx) * weight(ry, dx);

                if (w <= 0.0f) continue;

                MpmVec2 nodePos{ gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx,
                                 gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx };
                MpmVec2 diff = nodePos - p.pos;
                MpmVec2 apicContrib = p.C * diff;

                int idx = ni + nj * nx_;
                grid_[idx].mass += w * p.mass;
                grid_[idx].vel  += w * (p.mass * p.vel + apicContrib);
            }
        }
    }
}

// ---- MPM step: compute grid forces from particle stresses ----

void MpmSolver2D::computeGridForces() {
    float dx  = cellSize_;
    float inv = 1.0f / dx;

    for (auto& p : particles_) {
        if (!p.active) continue;

        MpmMat2 Fe = p.F * p.Fp.inverse();
        MpmMat2 P  = firstPiolaKirchhoff(Fe);

        float fx = (p.pos.x - gridOrigin_.x) * inv;
        float fy = (p.pos.y - gridOrigin_.y) * inv;
        int ix = static_cast<int>(std::floor(fx));
        int iy = static_cast<int>(std::floor(fy));

        for (int dj = -1; dj <= 2; ++dj) {
            for (int di = -1; di <= 2; ++di) {
                int ni = ix + di;
                int nj = iy + dj;
                if (ni < 0 || ni >= nx_ || nj < 0 || nj >= ny_) continue;

                float rx = p.pos.x - (gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx);
                float ry = p.pos.y - (gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx);
                float w  = weight(rx, dx);
                float dwx = dweight(rx, dx);
                float dwy = dweight(ry, dx);
                float gradW = dwx * weight(ry, dx); // dw/dx * N(y)
                float gradH = w * dwy;               // N(x) * dw/dy

                // force contribution: f_i -= V_p * P * grad(w)
                // (negative sign from variational derivative of elastic energy)
                float V = p.volume0;
                int idx = ni + nj * nx_;
                grid_[idx].force.x -= V * (P.m00 * gradW + P.m01 * gradH);
                grid_[idx].force.y -= V * (P.m10 * gradW + P.m11 * gradH);
            }
        }
    }
}

// ---- MPM step: update grid velocities (explicit) ----

void MpmSolver2D::updateGridVelocities(float dt) {
    for (int j = 0; j < ny_; ++j) {
        for (int i = 0; i < nx_; ++i) {
            int idx = i + j * nx_;
            auto& node = grid_[idx];
            if (node.mass <= 0.0f) continue;

            // v = (mv + dt * f_gravity + dt * f_internal) / m
            float invMass = 1.0f / node.mass;
            float gx = gravityX_;
            float gy = gravityY_;

            MpmVec2 accel{ gx + node.force.x * invMass,
                           gy + node.force.y * invMass };

            MpmVec2 vNew = (node.vel * invMass) + accel * dt;

            // damping (global)
            if (damping_ > 0.0f) {
                vNew *= (1.0f - damping_);
            }

            node.vel = vNew * node.mass; // store as momentum for G2P
        }
    }
}

// ---- MPM step: G2P with APIC ----

void MpmSolver2D::gridToParticle(float dt) {
    float dx  = cellSize_;
    float inv = 1.0f / dx;
    float dScale = 4.0f / (dx * dx); // scaling for APIC C update

    for (auto& p : particles_) {
        if (!p.active) continue;

        float fx = (p.pos.x - gridOrigin_.x) * inv;
        float fy = (p.pos.y - gridOrigin_.y) * inv;
        int ix = static_cast<int>(std::floor(fx));
        int iy = static_cast<int>(std::floor(fy));

        MpmVec2  newVel;
        MpmMat2  newC;  // zero initialised

        for (int dj = -1; dj <= 2; ++dj) {
            for (int di = -1; di <= 2; ++di) {
                int ni = ix + di;
                int nj = iy + dj;
                if (ni < 0 || ni >= nx_ || nj < 0 || nj >= ny_) continue;

                float rx = p.pos.x - (gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx);
                float ry = p.pos.y - (gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx);
                float w  = weight(rx, dx) * weight(ry, dx);

                if (w <= 0.0f) continue;

                int idx = ni + nj * nx_;
                auto& node = grid_[idx];
                if (node.mass <= 0.0f) continue;

                MpmVec2 vNode = node.vel / node.mass;
                newVel += w * vNode;

                MpmVec2 diff{ gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx - p.pos.x,
                              gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx - p.pos.y };

                // APIC C update: outer product * w * 4/dx^2
                newC.m00 += w * vNode.x * diff.x * dScale;
                newC.m01 += w * vNode.x * diff.y * dScale;
                newC.m10 += w * vNode.y * diff.x * dScale;
                newC.m11 += w * vNode.y * diff.y * dScale;
            }
        }

        p.vel = newVel;
        p.C   = newC;
        p.pos.x += newVel.x * dt;
        p.pos.y += newVel.y * dt;
    }
}

// ---- MPM step: update deformation gradient ----

void MpmSolver2D::updateDeformationGradient(float dt) {
    float dx  = cellSize_;
    float inv = 1.0f / dx;

    for (auto& p : particles_) {
        if (!p.active) continue;

        float fx = (p.pos.x - gridOrigin_.x) * inv;
        float fy = (p.pos.y - gridOrigin_.y) * inv;
        int ix = static_cast<int>(std::floor(fx));
        int iy = static_cast<int>(std::floor(fy));

        MpmMat2 gradV; // velocity gradient (dv/dx)

        for (int dj = -1; dj <= 2; ++dj) {
            for (int di = -1; di <= 2; ++di) {
                int ni = ix + di;
                int nj = iy + dj;
                if (ni < 0 || ni >= nx_ || nj < 0 || nj >= ny_) continue;

                float rx = p.pos.x - (gridOrigin_.x + (static_cast<float>(ni) + 0.5f) * dx);
                float ry = p.pos.y - (gridOrigin_.y + (static_cast<float>(nj) + 0.5f) * dx);
                float dwx = dweight(rx, dx);
                float dwy = dweight(ry, dx);
                float w  = weight(rx, dx);
                float gradW = dwx * weight(ry, dx);
                float gradH = w * dwy;

                int idx = ni + nj * nx_;
                auto& node = grid_[idx];
                if (node.mass <= 0.0f) continue;

                MpmVec2 vNode = node.vel / node.mass;
                gradV.m00 += vNode.x * gradW;
                gradV.m01 += vNode.x * gradH;
                gradV.m10 += vNode.y * gradW;
                gradV.m11 += vNode.y * gradH;
            }
        }

        // F_new = (I + dt * gradV) * F_old
        MpmMat2 dF{ 1.0f + dt * gradV.m00, dt * gradV.m01,
                    dt * gradV.m10, 1.0f + dt * gradV.m11 };
        p.F = dF * p.F;
    }
}

// ---- von Mises plasticity with hardening ----
// Project the singular values of Fe = F * Fp^{-1}

void MpmSolver2D::applyPlasticity() {
    for (auto& p : particles_) {
        if (!p.active) continue;

        // Fe = F * Fp^{-1}
        MpmMat2 FpInv = p.Fp.inverse();
        MpmMat2 Fe = p.F * FpInv;

        // SVD of Fe (2x2 analytic)
        // Fe = U * Sigma * V^T
        // https://en.wikipedia.org/wiki/Singular_value_decomposition#2_%C3%97_2_matrices

        float a = Fe.m00, b = Fe.m01;
        float c = Fe.m10, d = Fe.m11;

        // Compute SVD via the 2x2 formula
        float E = (a + d) * 0.5f;
        float F = (a - d) * 0.5f;
        float G = (b + c) * 0.5f;
        float H = (b - c) * 0.5f;

        float q = std::sqrt(E * E + H * H);
        float r = std::sqrt(F * F + G * G);

        // singular values
        float s1 = q + r;
        float s2 = q - r;

        // clamp to avoid negative singular values
        s1 = std::max(1e-6f, s1);
        s2 = std::max(1e-6f, s2);

        // von Mises: clamp deviatoric strain
        // equivalent strain = |log(s1)| + |log(s2)|
        float eps1 = std::log(s1);
        float eps2 = std::log(s2);
        float equivStrain = std::sqrt(eps1 * eps1 + eps2 * eps2);

        float yield = yieldStress_ / (mu_ * 3.0f); // normalised yield
        yield += hardening_ * p.plasticStrain;

        if (equivStrain > yield) {
            float scale = yield / equivStrain;
            float newEps1 = eps1 * scale;
            float newEps2 = eps2 * scale;

            // accumulated plastic strain increment
            float dPlastic = equivStrain - yield;
            p.plasticStrain += dPlastic * 0.5f; // averaged

            float newS1 = std::exp(newEps1);
            float newS2 = std::exp(newEps2);

            // Reconstruct Fe from SVD with clamped singular values
            // Fe = U * Sigma_new * V^T
            // But we need U and V ...

            // Using the E/F/G/H decomposition:
            // U = [[cos(theta), -sin(theta)], [sin(theta), cos(theta)]]
            // V = [[cos(phi), -sin(phi)], [sin(phi), cos(phi)]]
            // Actually for arbitrary F = U * diag(s1,s2) * V^T

            float theta = 0.0f, phi = 0.0f;

            if (r > 1e-12f) {
                float phiVal = 0.5f * std::atan2(G, F); // phi angle for V
                phi = phiVal;
            }
            if (q > 1e-12f) {
                float thetaVal = 0.5f * std::atan2(H, E); // theta angle for U
                theta = thetaVal;
            }

            float cT = std::cos(theta), sT = std::sin(theta);
            float cP = std::cos(phi),   sP = std::sin(phi);

            // U = [[cT, -sT], [sT, cT]]
            // V = [[cP, -sP], [sP, cP]]
            // Fe_new = U * diag(newS1, newS2) * V^T

            float newFe00 = newS1 * cT * cP + newS2 * sT * sP;
            float newFe01 = newS1 * cT * sP - newS2 * sT * cP;
            float newFe10 = newS1 * sT * cP - newS2 * cT * sP;
            float newFe11 = newS1 * sT * sP + newS2 * cT * cP;

            // Fe_new = F * Fp_new^{-1}
            // So Fp_new^{-1} = Fe_new^{-1} * F
            // Fp_new = F^{-1} * Fe_new ... wait

            // Actually: Fe_new = F * Fp_new^{-1}
            // So Fp_new^{-1} = F^{-1} * Fe_new
            // Fp_new = Fe_new^{-1} * F

            MpmMat2 FeNew{newFe00, newFe01, newFe10, newFe11};
            MpmMat2 FnewInv = p.F.inverse();

            // Fp_inv_new = F^{-1} * Fe_new
            MpmMat2 FpInvNew = FnewInv * FeNew;
            p.Fp = FpInvNew.inverse();
            p.F  = FeNew * p.Fp; // ensure F = Fe * Fp
        }
    }
}

// ---- fracture ----

void MpmSolver2D::checkFracture() {
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        auto& p = particles_[i];
        if (!p.active) continue;

        // fracture when accumulated plastic strain exceeds threshold
        if (p.plasticStrain >= maxPlasticStrain_) {
            p.active = false;
            fracturedIndices_.push_back(i);
        }

        // fracture energy criterion: tensile failure
        if (fractureEnergy_ > 0.0f) {
            MpmMat2 Fe = p.F * p.Fp.inverse();
            float J  = Fe.det();
            // volumetric strain energy
            float energy = 0.5f * lambda_ * (J - 1.0f) * (J - 1.0f)
                         + mu_ * ((Fe.m00 - 1.0f) * (Fe.m00 - 1.0f)
                                + Fe.m01 * Fe.m01
                                + Fe.m10 * Fe.m10
                                + (Fe.m11 - 1.0f) * (Fe.m11 - 1.0f));
            if (p.active && energy >= fractureEnergy_ * mu_) {
                p.active = false;
                fracturedIndices_.push_back(i);
            }
        }
    }
}

// ---- boundary conditions ----

void MpmSolver2D::applyBoundaryConditions() {
    if (!hasBoundary_) return;

    for (int j = 0; j < ny_; ++j) {
        for (int i = 0; i < nx_; ++i) {
            int idx = i + j * nx_;
            auto& node = grid_[idx];
            if (node.mass <= 0.0f) continue;

            MpmVec2 pos{ gridOrigin_.x + (static_cast<float>(i) + 0.5f) * cellSize_,
                         gridOrigin_.y + (static_cast<float>(j) + 0.5f) * cellSize_ };

            MpmVec2 v = node.vel / node.mass;

            // left
            if (pos.x - cellSize_ * 0.5f <= bxmin_) {
                v.x = std::max(0.0f, v.x + boundaryFriction_ * v.x);
                v.y *= (1.0f - boundaryFriction_);
            }
            // right
            if (pos.x + cellSize_ * 0.5f >= bxmax_) {
                v.x = std::min(0.0f, v.x - boundaryFriction_ * v.x);
                v.y *= (1.0f - boundaryFriction_);
            }
            // bottom
            if (pos.y - cellSize_ * 0.5f <= bymin_) {
                v.y = std::max(0.0f, v.y + boundaryFriction_ * v.y);
                v.x *= (1.0f - boundaryFriction_);
            }
            // top
            if (pos.y + cellSize_ * 0.5f >= bymax_) {
                v.y = std::min(0.0f, v.y - boundaryFriction_ * v.y);
                v.x *= (1.0f - boundaryFriction_);
            }

            node.vel = v * node.mass;
        }
    }
}

void MpmSolver2D::resolveColliders() {
    for (auto& particle : particles_) {
        if (!particle.active) continue;
        for (const auto& collider : colliders_) {
            if (!collider.enabled) continue;
            const float friction = std::clamp(collider.friction, 0.0f, 1.0f);
            const float restitution = std::clamp(collider.restitution, 0.0f, 1.0f);
            if (collider.type == MpmCollider2D::Type::Plane) {
                if (particle.pos.y < collider.y) {
                    particle.pos.y = collider.y;
                    particle.vel.y = std::abs(particle.vel.y) * restitution;
                    particle.vel.x *= 1.0f - friction;
                }
                continue;
            }
            if (collider.type == MpmCollider2D::Type::Circle) {
                const float dx = particle.pos.x - collider.x;
                const float dy = particle.pos.y - collider.y;
                const float radius = std::max(0.0f, collider.radius);
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq >= radius * radius || distanceSq <= 1e-8f) continue;
                const float distance = std::sqrt(distanceSq);
                const float nx = dx / distance;
                const float ny = dy / distance;
                particle.pos.x = collider.x + nx * radius;
                particle.pos.y = collider.y + ny * radius;
                const float normalVelocity = particle.vel.x * nx + particle.vel.y * ny;
                if (normalVelocity < 0.0f) {
                    particle.vel.x -= (1.0f + restitution) * normalVelocity * nx;
                    particle.vel.y -= (1.0f + restitution) * normalVelocity * ny;
                }
                particle.vel.x *= 1.0f - friction;
                particle.vel.y *= 1.0f - friction;
                continue;
            }
            const float halfWidth = std::max(0.0f, collider.width * 0.5f);
            const float halfHeight = std::max(0.0f, collider.height * 0.5f);
            const float minX = collider.x - halfWidth;
            const float maxX = collider.x + halfWidth;
            const float minY = collider.y - halfHeight;
            const float maxY = collider.y + halfHeight;
            if (particle.pos.x < minX || particle.pos.x > maxX ||
                particle.pos.y < minY || particle.pos.y > maxY) continue;
            const float left = particle.pos.x - minX;
            const float right = maxX - particle.pos.x;
            const float bottom = particle.pos.y - minY;
            const float top = maxY - particle.pos.y;
            const float minimum = std::min({left, right, bottom, top});
            if (minimum == left || minimum == right) {
                particle.pos.x = minimum == left ? minX : maxX;
                particle.vel.x = (minimum == left ? -1.0f : 1.0f) *
                                 std::abs(particle.vel.x) * restitution;
                particle.vel.y *= 1.0f - friction;
            } else {
                particle.pos.y = minimum == bottom ? minY : maxY;
                particle.vel.y = (minimum == bottom ? -1.0f : 1.0f) *
                                 std::abs(particle.vel.y) * restitution;
                particle.vel.x *= 1.0f - friction;
            }
        }
    }
}

// ---- impulse / body force ----

void MpmSolver2D::addImpulse(float x, float y, float force, float radius) {
    float rSq = radius * radius;
    for (auto& p : particles_) {
        if (!p.active) continue;
        float dx = p.pos.x - x;
        float dy = p.pos.y - y;
        float dSq = dx * dx + dy * dy;
        if (dSq > rSq) continue;
        float falloff = 1.0f - dSq / rSq;
        p.vel.x += force * falloff * (dx > 0 ? 1.0f : -1.0f) * 0.01f;
        p.vel.y += force * falloff * (dy > 0 ? 1.0f : -1.0f) * 0.01f;
    }
}

void MpmSolver2D::addBodyForce(float fx, float fy) {
    for (auto& p : particles_) {
        if (!p.active) continue;
        p.vel.x += fx;
        p.vel.y += fy;
    }
}

// ---- main update ----

void MpmSolver2D::update(float elapsedSeconds) {
    if (elapsedSeconds <= 0.0f) return;
    const float maxAccumulated = fixedDt_ * static_cast<float>(maxSubsteps_);
    accumulatedTime_ = std::min(accumulatedTime_ +
                                    std::min(elapsedSeconds, maxAccumulated),
                                maxAccumulated);
    int substeps = 0;
    while (accumulatedTime_ + 1e-8f >= fixedDt_ &&
           substeps < maxSubsteps_) {
        stepOnce(fixedDt_);
        accumulatedTime_ -= fixedDt_;
        ++substeps;
    }
}

void MpmSolver2D::stepOnce(float h) {

    resetGrid();
    particleToGrid();
    applyBoundaryConditions();
    computeGridForces();
    updateGridVelocities(h);
    applyBoundaryConditions();
    gridToParticle(h);
    updateDeformationGradient(h);
    applyPlasticity();
    resolveColliders();
    checkFracture();

    // particle-level boundary
    if (hasBoundary_) {
        float margin = cellSize_ * 0.5f;
        for (auto& p : particles_) {
            if (!p.active) continue;
            if (p.pos.x < bxmin_ + margin) { p.pos.x = bxmin_ + margin; p.vel.x = 0; }
            if (p.pos.x > bxmax_ - margin) { p.pos.x = bxmax_ - margin; p.vel.x = 0; }
            if (p.pos.y < bymin_ + margin) { p.pos.y = bymin_ + margin; p.vel.y = 0; }
            if (p.pos.y > bymax_ - margin) { p.pos.y = bymax_ - margin; p.vel.y = 0; }
        }
    }
}

} // namespace ArtifactCore
