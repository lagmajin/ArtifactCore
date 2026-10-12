module;
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
module Physics.Mpm2D;
import Core.Parallel;

import Graphics.MpmCompute;
import Graphics.GPUcomputeContext;

namespace ArtifactCore {

namespace {

struct MpmPositiveSvd2D {
    float singular0 = 0.0f;
    float singular1 = 0.0f;
    float u0x = 1.0f;
    float u0y = 0.0f;
    float v0x = 1.0f;
    float v0y = 0.0f;
    bool valid = false;
};

struct MpmPositiveSingularValues2D {
    float singular0 = 0.0f;
    float singular1 = 0.0f;
    bool valid = false;
};

MpmPositiveSvd2D decomposePositiveMpm2D(const MpmMat2& matrix) noexcept {
    MpmPositiveSvd2D result;
    if (!std::isfinite(matrix.m00) || !std::isfinite(matrix.m01) ||
        !std::isfinite(matrix.m10) || !std::isfinite(matrix.m11)) {
        return result;
    }

    // Scale before forming the Gram matrix to avoid square overflow. Computing
    // the smaller eigenvalue as mean - radius loses it for nearly isotropic
    // stretches, so recover the smaller singular value from the scaled
    // determinant instead of subtracting nearly equal values.
    const float principalScale = std::hypot(
        std::hypot(matrix.m00, matrix.m10),
        std::hypot(matrix.m01, matrix.m11));
    if (!std::isfinite(principalScale) || principalScale <= 0.0f) {
        return result;
    }
    const float scaledDeterminant =
        (matrix.m00 / principalScale) * (matrix.m11 / principalScale) -
        (matrix.m01 / principalScale) * (matrix.m10 / principalScale);
    if (!std::isfinite(scaledDeterminant) || scaledDeterminant <= 0.0f) {
        return result;
    }
    const float scaled00 = matrix.m00 / principalScale;
    const float scaled01 = matrix.m01 / principalScale;
    const float scaled10 = matrix.m10 / principalScale;
    const float scaled11 = matrix.m11 / principalScale;
    const float c00 = scaled00 * scaled00 + scaled10 * scaled10;
    const float c01 = scaled00 * scaled01 + scaled10 * scaled11;
    const float c11 = scaled01 * scaled01 + scaled11 * scaled11;
    const float halfDifference = (c00 - c11) * 0.5f;
    const float radius = std::hypot(halfDifference, c01);
    const float eigenvalue0 = (c00 + c11) * 0.5f + radius;
    if (!std::isfinite(eigenvalue0) || eigenvalue0 <= 1.0e-16f) {
        return result;
    }

    result.singular0 = principalScale * std::sqrt(eigenvalue0);
    result.singular1 = principalScale * scaledDeterminant /
                       std::sqrt(eigenvalue0);
    const float angle = 0.5f * std::atan2(2.0f * c01, c00 - c11);
    result.v0x = std::cos(angle);
    result.v0y = std::sin(angle);
    const float leftX = matrix.m00 * result.v0x + matrix.m01 * result.v0y;
    const float leftY = matrix.m10 * result.v0x + matrix.m11 * result.v0y;
    const float leftLength = std::hypot(leftX, leftY);
    if (!std::isfinite(result.singular0) ||
        !std::isfinite(result.singular1) ||
        !std::isfinite(leftLength) || leftLength <= 1.0e-12f) {
        return MpmPositiveSvd2D{};
    }
    result.u0x = leftX / leftLength;
    result.u0y = leftY / leftLength;
    result.valid = true;
    return result;
}

MpmPositiveSingularValues2D positiveMpmSingularValues2D(
    const MpmMat2& matrix) noexcept {
    MpmPositiveSingularValues2D result;
    if (!std::isfinite(matrix.m00) || !std::isfinite(matrix.m01) ||
        !std::isfinite(matrix.m10) || !std::isfinite(matrix.m11)) {
        return result;
    }

    const float principalScale = std::hypot(
        std::hypot(matrix.m00, matrix.m10),
        std::hypot(matrix.m01, matrix.m11));
    if (!std::isfinite(principalScale) || principalScale <= 0.0f) {
        return result;
    }
    const float scaledDeterminant =
        (matrix.m00 / principalScale) * (matrix.m11 / principalScale) -
        (matrix.m01 / principalScale) * (matrix.m10 / principalScale);
    if (!std::isfinite(scaledDeterminant) || scaledDeterminant <= 0.0f) {
        return result;
    }

    const float scaled00 = matrix.m00 / principalScale;
    const float scaled01 = matrix.m01 / principalScale;
    const float scaled10 = matrix.m10 / principalScale;
    const float scaled11 = matrix.m11 / principalScale;
    const float c00 = scaled00 * scaled00 + scaled10 * scaled10;
    const float c01 = scaled00 * scaled01 + scaled10 * scaled11;
    const float c11 = scaled01 * scaled01 + scaled11 * scaled11;
    const float halfDifference = (c00 - c11) * 0.5f;
    const float radius = std::hypot(halfDifference, c01);
    const float eigenvalue0 = (c00 + c11) * 0.5f + radius;
    if (!std::isfinite(eigenvalue0) || eigenvalue0 <= 1.0e-16f) {
        return result;
    }

    const float rootEigenvalue0 = std::sqrt(eigenvalue0);
    result.singular0 = principalScale * rootEigenvalue0;
    result.singular1 = principalScale * scaledDeterminant / rootEigenvalue0;
    result.valid = std::isfinite(result.singular0) &&
                   std::isfinite(result.singular1) &&
                   result.singular0 > 0.0f && result.singular1 > 0.0f;
    return result;
}

double granularHenckyAcousticSpeedSquaredUpperBound(
    double sigma0, double sigma1, double lameLambda,
    double shearModulus) noexcept {
    if (!std::isfinite(sigma0) || !std::isfinite(sigma1) ||
        !std::isfinite(lameLambda) || !std::isfinite(shearModulus) ||
        sigma0 <= 0.0 || sigma1 <= 0.0 || shearModulus <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double log0 = std::log(sigma0);
    const double log1 = std::log(sigma1);
    const double tau0 = 2.0 * shearModulus * log0 +
                        lameLambda * (log0 + log1);
    const double tau1 = 2.0 * shearModulus * log1 +
                        lameLambda * (log0 + log1);
    const double referenceLongitudinal = lameLambda + 2.0 * shearModulus;
    if (!std::isfinite(tau0) || !std::isfinite(tau1) ||
        !std::isfinite(referenceLongitudinal) ||
        referenceLongitudinal <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // In the elastic principal basis, the weighted longitudinal tangent
    // entries are sigma_i^2 * d^2(psi)/d(sigma_i)^2. The off-diagonal
    // shear tangent has the repeated-stretch limit mu / sigma^2.
    const double stretchDifference = sigma0 - sigma1;
    double shearFactor = 0.0;
    if (stretchDifference == 0.0) {
        shearFactor = shearModulus / (sigma0 * sigma0);
    } else {
        const double squaredDifference =
            stretchDifference * (sigma0 + sigma1);
        shearFactor = 2.0 * shearModulus *
                      std::log1p(stretchDifference / sigma1) /
                      squaredDifference;
    }
    if (!std::isfinite(shearFactor)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double longitudinal0 = referenceLongitudinal - tau0;
    const double longitudinal1 = referenceLongitudinal - tau1;
    const double shear0 = shearFactor * sigma1 * sigma1;
    const double shear1 = shearFactor * sigma0 * sigma0;
    const double offDiagonal = lameLambda + shearFactor * sigma0 * sigma0 - tau0;

    // For a current-space propagation direction n, the 2x2 acoustic tensor
    // has diagonal entries that are affine in cos^2(theta), plus an off-
    // diagonal term proportional to sin(theta)cos(theta). Gershgorin row
    // sums bound its largest eigenvalue for every direction. Maximizing each
    // row sum over theta has the closed form below.
    const double row0Upper = 0.5 * (
        longitudinal0 + shear0 +
        std::hypot(longitudinal0 - shear0, offDiagonal));
    const double row1Upper = 0.5 * (
        shear1 + longitudinal1 +
        std::hypot(shear1 - longitudinal1, offDiagonal));
    const double upperBound =
        std::max({referenceLongitudinal, row0Upper, row1Upper});
    return std::isfinite(upperBound)
               ? upperBound
               : std::numeric_limits<double>::quiet_NaN();
}

float hardenedMpmFrictionAlpha2D(float initialAngleRadians,
                                 float hardeningState) noexcept {
    // Adapt the published sand hardening curve so q=0 starts at the layer's
    // configured angle: h0=phi0+h3, with h1=9 deg, h2=0.2, h3=10 deg.
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    constexpr float kHardeningH1 = 9.0f * kDegreesToRadians;
    constexpr float kHardeningH2 = 0.2f;
    constexpr float kHardeningH3 = 10.0f * kDegreesToRadians;
    constexpr float kMaximumFrictionAngle = 89.0f * kDegreesToRadians;
    const float q = std::clamp(hardeningState, 0.0f, 100.0f);
    const float asymptoticAngle = initialAngleRadians + kHardeningH3;
    const float frictionAngle = asymptoticAngle +
        (kHardeningH1 * q - kHardeningH3) * std::exp(-kHardeningH2 * q);
    const float boundedAngle = std::clamp(
        frictionAngle, initialAngleRadians, kMaximumFrictionAngle);
    // In 2D, the Coulomb-to-Drucker-Prager conversion is
    // alpha = mu / sqrt(2 * (1 + mu^2)); with mu = tan(phi),
    // this reduces to sin(phi) / sqrt(2).
    return std::sin(boundedAngle) * 0.70710678118654752440f;
}

} // namespace

// ---- GPU session (opaque behind void* in the interface) ----

struct MpmGpuSession {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    std::unique_ptr<GpuContext> gpuContext;
    std::unique_ptr<MpmGPUCompute> compute;
    std::uint32_t capacity = 0;
    std::uint32_t gridNx = 0;
    std::uint32_t gridNy = 0;
    bool initialized = false;
    bool failed = false;
};

// ---- static helpers ----

MpmMat2 MpmMat2::identity() noexcept { return {1,0,0,1}; }
MpmMat2 MpmMat2::zero()     noexcept { return {0,0,0,0}; }

MpmHenckyProjection2D projectMpmGranularHencky2D(
    float logStretch0, float logStretch1, float lameLambda,
    float shearModulus, float frictionAlpha) noexcept {
    MpmHenckyProjection2D result;
    const float bulkStability = lameLambda + shearModulus;
    if (!std::isfinite(logStretch0) || !std::isfinite(logStretch1) ||
        !std::isfinite(lameLambda) || !std::isfinite(shearModulus) ||
        !std::isfinite(frictionAlpha) || shearModulus <= 0.0f ||
        !std::isfinite(bulkStability) || bulkStability <= 0.0f ||
        frictionAlpha < 0.0f ||
        frictionAlpha >= 1.0f) {
        return result;
    }

    result.logStretch0 = logStretch0;
    result.logStretch1 = logStretch1;
    const float trace = logStretch0 + logStretch1;
    if (!std::isfinite(trace)) return result;
    result.valid = true;

    // The 2016 sand model projects extension states to the Drucker-Prager
    // cone tip. Keep this branch before the deviatoric norm division.
    if (trace >= 0.0f) {
        result.logStretch0 = 0.0f;
        result.logStretch1 = 0.0f;
        result.plasticMultiplier = std::hypot(logStretch0, logStretch1);
        if (!std::isfinite(result.plasticMultiplier)) {
            result.valid = false;
            return result;
        }
        result.projected = true;
        result.extensionTip = true;
        return result;
    }

    const float mean = trace * 0.5f;
    const float dev0 = logStretch0 - mean;
    const float dev1 = logStretch1 - mean;
    const float devNorm = std::hypot(dev0, dev1);
    const float plasticMultiplier = devNorm +
        (1.0f + lameLambda / shearModulus) * trace * frictionAlpha;
    if (!std::isfinite(plasticMultiplier)) {
        result.valid = false;
        return result;
    }
    if (plasticMultiplier <= 0.0f) {
        return result;
    }

    if (devNorm <= 1.0e-12f) {
        // For negative trace and non-negative friction, this should only be
        // reachable through roundoff at the hydrostatic yield apex.
        result.valid = false;
        return result;
    }

    const float scale = plasticMultiplier / devNorm;
    result.logStretch0 = logStretch0 - scale * dev0;
    result.logStretch1 = logStretch1 - scale * dev1;
    result.plasticMultiplier = plasticMultiplier;
    result.projected = true;
    result.valid = std::isfinite(result.logStretch0) &&
                   std::isfinite(result.logStretch1);
    return result;
}

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
    if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
        const MpmPositiveSvd2D svd = decomposePositiveMpm2D(Fe);
        if (!svd.valid || svd.singular0 <= 1.0e-8f ||
            svd.singular1 <= 1.0e-8f) {
            return MpmMat2::zero();
        }

        const float logS0 = std::log(svd.singular0);
        const float logS1 = std::log(svd.singular1);
        const float trace = logS0 + logS1;
        const float tau0 = 2.0f * mu_ * logS0 + lambda_ * trace;
        const float tau1 = 2.0f * mu_ * logS1 + lambda_ * trace;
        const float p0 = tau0 / svd.singular0;
        const float p1 = tau1 / svd.singular1;
        if (!std::isfinite(p0) || !std::isfinite(p1)) {
            return MpmMat2::zero();
        }
        const float u1x = -svd.u0y, u1y = svd.u0x;
        const float v1x = -svd.v0y, v1y = svd.v0x;
        const MpmMat2 stress{
            p0 * svd.u0x * svd.v0x + p1 * u1x * v1x,
            p0 * svd.u0x * svd.v0y + p1 * u1x * v1y,
            p0 * svd.u0y * svd.v0x + p1 * u1y * v1x,
            p0 * svd.u0y * svd.v0y + p1 * u1y * v1y};
        return std::isfinite(stress.m00) && std::isfinite(stress.m01) &&
                       std::isfinite(stress.m10) && std::isfinite(stress.m11)
                   ? stress
                   : MpmMat2::zero();
    }

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

void MpmSolver2D::resetGrid() {
    const std::size_t activeNodeCount =
        static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_);
    Parallel::ForSize(0, activeNodeCount, activeNodeCount, [&](std::size_t index) {
        auto& node = grid_[index];
        node.mass  = 0.0f;
        node.vel   = MpmVec2{};
        node.force = MpmVec2{};
    });
}

bool MpmSolver2D::setGrid(float cellSize, int nx, int ny) {
    if (!particles_.empty() || !std::isfinite(cellSize)) return false;
    const float safeCellSize = std::max(0.1f, cellSize);
    const int safeNx = std::max(2, nx);
    const int safeNy = std::max(2, ny);
    if (safeNx > std::numeric_limits<int>::max() / safeNy) return false;
    const std::size_t nodeCount = static_cast<std::size_t>(safeNx) *
                                  static_cast<std::size_t>(safeNy);
    if (nodeCount > grid_.max_size()) return false;
    const double maxFloat = std::numeric_limits<float>::max();
    const double extentX = static_cast<double>(safeNx) * safeCellSize;
    const double extentY = static_cast<double>(safeNy) * safeCellSize;
    const double farX = static_cast<double>(gridOrigin_.x) + extentX;
    const double farY = static_cast<double>(gridOrigin_.y) + extentY;
    if (!std::isfinite(extentX) || !std::isfinite(extentY) ||
        !std::isfinite(farX) || !std::isfinite(farY) ||
        std::abs(farX) > maxFloat || std::abs(farY) > maxFloat) {
        return false;
    }
    grid_.resize(nodeCount);
    cellSize_ = safeCellSize;
    nx_ = safeNx;
    ny_ = safeNy;
    gridExpansionLimitReached_ = false;
    return true;
}

bool MpmSolver2D::reserveGridNodeCapacity(std::size_t maximumNodes) {
    if (!particles_.empty() || nx_ <= 0 || ny_ <= 0 ||
        static_cast<std::size_t>(nx_) > maximumNodes /
            static_cast<std::size_t>(ny_) ||
        maximumNodes > grid_.max_size()) {
        return false;
    }
    try {
        grid_.resize(maximumNodes);
    } catch (const std::bad_alloc&) {
        return false;
    }
    gridExpansionLimitReached_ = false;
    return true;
}

void MpmSolver2D::setMaterial(float youngModulus, float poissonRatio) {
    E_  = std::max(1.0f, youngModulus);
    nu_ = std::clamp(poissonRatio, -0.99f, 0.49f);
    mu_     = E_ / (2.0f * (1.0f + nu_));
    lambda_ = E_ * nu_ / ((1.0f + nu_) * (1.0f - 2.0f * nu_));
}

bool MpmSolver2D::setConstitutiveModel(MpmConstitutiveModel model) noexcept {
    if (model != MpmConstitutiveModel::CorotatedVonMises &&
        model != MpmConstitutiveModel::GranularDruckerPrager) {
        return false;
    }
    if (model == MpmConstitutiveModel::GranularDruckerPrager &&
        backend_ == MpmBackend::GPU) {
        return false;
    }
    if (model != constitutiveModel_ && !particles_.empty()) {
        return false;
    }
    constitutiveModel_ = model;
    return true;
}

bool MpmSolver2D::setGranularFrictionAngle(float radians) noexcept {
    constexpr float kHalfPi = 1.57079632679f;
    if (!std::isfinite(radians) || radians < 0.0f ||
        radians >= kHalfPi - 1.0e-3f) {
        return false;
    }
    if (!particles_.empty() && radians != granularFrictionAngle_) {
        return false;
    }
    granularFrictionAngle_ = radians;
    return true;
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
    hasGranularFloor_ = false;
    if (!std::isfinite(xmin) || !std::isfinite(ymin) ||
        !std::isfinite(xmax) || !std::isfinite(ymax)) {
        hasBoundary_ = false;
        return;
    }
    const float minimumX = std::min(xmin, xmax);
    const float minimumY = std::min(ymin, ymax);
    const float maximumX = std::max(xmin, xmax);
    const float maximumY = std::max(ymin, ymax);
    const double width =
        static_cast<double>(maximumX) - static_cast<double>(minimumX);
    const double height =
        static_cast<double>(maximumY) - static_cast<double>(minimumY);
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width > std::numeric_limits<float>::max() ||
        height > std::numeric_limits<float>::max() ||
        width < 1.0e-6 || height < 1.0e-6) {
        hasBoundary_ = false;
        return;
    }
    bxmin_ = minimumX;
    bymin_ = minimumY;
    bxmax_ = maximumX;
    bymax_ = maximumY;
    hasBoundary_ = true;
}

bool MpmSolver2D::setGranularFloor(float y) noexcept {
    if (constitutiveModel_ !=
            MpmConstitutiveModel::GranularDruckerPrager ||
        !std::isfinite(y)) {
        return false;
    }
    hasBoundary_ = false;
    granularFloorY_ = y;
    hasGranularFloor_ = true;
    return true;
}

bool MpmSolver2D::setGranularSupportRadiusLimit(float radius) noexcept {
    if (constitutiveModel_ != MpmConstitutiveModel::GranularDruckerPrager ||
        !particles_.empty() || !std::isfinite(radius) || radius <= 0.0f) {
        return false;
    }
    granularSupportRadiusLimit_ = radius;
    return true;
}

void MpmSolver2D::clear() {
    particles_.clear();
    particleBoundsCache_ = {};
    particleBoundsCacheValid_ = true;
    fracturedIndices_.clear();
    clearColliders();
    accumulatedTime_ = 0.0f;
    particleGridColumns_ = 0;
    particleGridRows_ = 0;
    gridExpansionLimitReached_ = false;
}

bool MpmSolver2D::writeDensityField(
    float* output, std::size_t outputCapacity, int width, int height,
    float worldLeft, float worldTop, float worldWidth, float worldHeight,
    float supportRadius) const noexcept {
    constexpr std::size_t kMaximumDensityFieldPixels = 4u * 1024u * 1024u;
    if (!output || width <= 0 || height <= 0 || width > 4096 ||
        height > 4096 || !std::isfinite(worldLeft) ||
        !std::isfinite(worldTop) || !std::isfinite(worldWidth) ||
        !std::isfinite(worldHeight) || worldWidth <= 0.0f ||
        worldHeight <= 0.0f ||
        (constitutiveModel_ != MpmConstitutiveModel::GranularDruckerPrager &&
         (!std::isfinite(supportRadius) || supportRadius <= 0.0f))) {
        return false;
    }
    const std::size_t fieldWidth = static_cast<std::size_t>(width);
    const std::size_t fieldHeight = static_cast<std::size_t>(height);
    if (fieldWidth > kMaximumDensityFieldPixels / fieldHeight) return false;
    const std::size_t pixelCount = fieldWidth * fieldHeight;
    if (outputCapacity < pixelCount) return false;

    const float pixelWorldWidth = worldWidth / static_cast<float>(width);
    const float pixelWorldHeight = worldHeight / static_cast<float>(height);
    const bool granularSand = constitutiveModel_ ==
        MpmConstitutiveModel::GranularDruckerPrager;
    const float maximumSupportRadius =
        8.0f * std::min(pixelWorldWidth, pixelWorldHeight);
    const float maximumSupportRadiusX = 8.0f * pixelWorldWidth;
    const float maximumSupportRadiusY = 8.0f * pixelWorldHeight;
    const float worldRight = worldLeft + worldWidth;
    const float worldBottom = worldTop + worldHeight;
    if (!std::isfinite(pixelWorldWidth) || pixelWorldWidth <= 0.0f ||
        !std::isfinite(pixelWorldHeight) || pixelWorldHeight <= 0.0f ||
        !std::isfinite(worldRight) || worldRight <= worldLeft ||
        !std::isfinite(worldBottom) || worldBottom <= worldTop ||
        !std::isfinite(maximumSupportRadius) ||
        maximumSupportRadius <= 0.0f ||
        !std::isfinite(maximumSupportRadiusX) ||
        !std::isfinite(maximumSupportRadiusY)) {
        return false;
    }

    std::fill_n(output, pixelCount, 0.0f);
    const float requestedSupportRadius =
        granularSand
            ? granularSupportRadiusLimit_
            : supportRadius;
    const float boundedSupportRadius =
        std::min(requestedSupportRadius, maximumSupportRadius);
    const float rasterSupportFloor = granularSand
        ? std::hypot(pixelWorldWidth, pixelWorldHeight) * (1.0f + 1.0e-4f)
        : 0.0f;
    const float minimumRasterSupportRadiusX = granularSand
        ? std::min(maximumSupportRadiusX, rasterSupportFloor)
        : 0.0f;
    const float minimumRasterSupportRadiusY = granularSand
        ? std::min(maximumSupportRadiusY, rasterSupportFloor)
        : 0.0f;
    constexpr float kTwoOverPi = 0.63661977236758134308f;
    for (const auto& particle : particles_) {
        if (!particle.active || !std::isfinite(particle.pos.x) ||
            !std::isfinite(particle.pos.y) ||
            !std::isfinite(particle.volume0) || particle.volume0 <= 0.0f) {
            continue;
        }
        const float deformationDeterminant = particle.F.det();
        if (!std::isfinite(deformationDeterminant) ||
            deformationDeterminant <= 0.0f) {
            continue;
        }
        const float currentVolume = particle.volume0 * deformationDeterminant;
        if (!std::isfinite(currentVolume) || currentVolume <= 0.0f) continue;
        const float deformedSupportRadius = boundedSupportRadius *
            std::sqrt(deformationDeterminant);
        const float particleSupportRadius = granularSand
            ? 0.0f
            : std::min(deformedSupportRadius, maximumSupportRadius);
        const float particleSupportRadiusX = granularSand
            ? std::min(maximumSupportRadiusX,
                       std::max(minimumRasterSupportRadiusX,
                                deformedSupportRadius))
            : particleSupportRadius;
        const float particleSupportRadiusY = granularSand
            ? std::min(maximumSupportRadiusY,
                       std::max(minimumRasterSupportRadiusY,
                                deformedSupportRadius))
            : particleSupportRadius;
        const float particleRadiusSquared = particleSupportRadius *
                                            particleSupportRadius;
        if (!std::isfinite(particleSupportRadiusX) ||
            particleSupportRadiusX <= 0.0f ||
            !std::isfinite(particleSupportRadiusY) ||
            particleSupportRadiusY <= 0.0f ||
            (!granularSand &&
             (!std::isfinite(particleRadiusSquared) ||
              particleRadiusSquared <= 0.0f))) {
            continue;
        }
        const float particleRadiusInPixelsX =
            particleSupportRadiusX / pixelWorldWidth;
        const float particleRadiusInPixelsY =
            particleSupportRadiusY / pixelWorldHeight;
        const float centerX =
            (particle.pos.x - worldLeft) / pixelWorldWidth - 0.5f;
        const float centerY =
            (particle.pos.y - worldTop) / pixelWorldHeight - 0.5f;
        if (!std::isfinite(centerX) || !std::isfinite(centerY)) continue;
        if (centerX < -particleRadiusInPixelsX ||
            centerX > static_cast<float>(width) + particleRadiusInPixelsX ||
            centerY < -particleRadiusInPixelsY ||
            centerY > static_cast<float>(height) + particleRadiusInPixelsY) {
            continue;
        }
        const int minX = std::max(0, static_cast<int>(std::floor(
            centerX - particleRadiusInPixelsX)));
        const int maxX = std::min(width - 1, static_cast<int>(std::ceil(
            centerX + particleRadiusInPixelsX)));
        const int minY = std::max(0, static_cast<int>(std::floor(
            centerY - particleRadiusInPixelsY)));
        const int maxY = std::min(height - 1, static_cast<int>(std::ceil(
            centerY + particleRadiusInPixelsY)));
        if (granularSand) {
            constexpr int kMaximumRasterStencilWidth = 18;
            constexpr int kMaximumRasterStencilSamples =
                kMaximumRasterStencilWidth * kMaximumRasterStencilWidth;
            const int stencilWidth = maxX - minX + 1;
            const int stencilHeight = maxY - minY + 1;
            if (stencilWidth <= 0 || stencilHeight <= 0 ||
                stencilWidth > kMaximumRasterStencilWidth ||
                stencilHeight > kMaximumRasterStencilWidth) {
                continue;
            }
            float kernelWeights[kMaximumRasterStencilSamples];
            double kernelWeightSum = 0.0;
            for (int localY = 0; localY < stencilHeight; ++localY) {
                const int y = minY + localY;
                const float sampleY = worldTop +
                    (static_cast<float>(y) + 0.5f) * pixelWorldHeight;
                const float deltaY = sampleY - particle.pos.y;
                for (int localX = 0; localX < stencilWidth; ++localX) {
                    const int x = minX + localX;
                    const float sampleX = worldLeft +
                        (static_cast<float>(x) + 0.5f) * pixelWorldWidth;
                    const float deltaX = sampleX - particle.pos.x;
                    const float normalizedX =
                        deltaX / particleSupportRadiusX;
                    const float normalizedY =
                        deltaY / particleSupportRadiusY;
                    const float normalizedDistanceSquared =
                        normalizedX * normalizedX + normalizedY * normalizedY;
                    const float kernel = normalizedDistanceSquared < 1.0f
                        ? 1.0f - normalizedDistanceSquared
                        : 0.0f;
                    kernelWeights[localY * stencilWidth + localX] = kernel;
                    kernelWeightSum += kernel;
                }
            }
            const double pixelArea =
                static_cast<double>(pixelWorldWidth) * pixelWorldHeight;
            if (!std::isfinite(kernelWeightSum) || kernelWeightSum <= 0.0 ||
                !std::isfinite(pixelArea) || pixelArea <= 0.0) {
                continue;
            }
            const double normalizedPeakDensity =
                static_cast<double>(currentVolume) /
                (pixelArea * kernelWeightSum);
            const float peakDensity = static_cast<float>(std::min(
                static_cast<double>(std::numeric_limits<float>::max()),
                normalizedPeakDensity));
            if (!std::isfinite(peakDensity) || peakDensity <= 0.0f) continue;
            for (int localY = 0; localY < stencilHeight; ++localY) {
                const int y = minY + localY;
                for (int localX = 0; localX < stencilWidth; ++localX) {
                    const float kernelWeightsValue =
                        kernelWeights[localY * stencilWidth + localX];
                    if (kernelWeightsValue <= 0.0f) continue;
                    const int x = minX + localX;
                    const std::size_t index =
                        static_cast<std::size_t>(y) * fieldWidth +
                        static_cast<std::size_t>(x);
                    output[index] = std::min(
                        1.0f, output[index] +
                                  peakDensity * kernelWeightsValue);
                }
            }
            continue;
        }
        const float peakDensity = std::min(
            1.0f, currentVolume * kTwoOverPi / particleRadiusSquared);
        if (!std::isfinite(peakDensity) || peakDensity <= 0.0f) continue;
        for (int y = minY; y <= maxY; ++y) {
            const float sampleY = worldTop +
                (static_cast<float>(y) + 0.5f) * pixelWorldHeight;
            const float deltaY = sampleY - particle.pos.y;
            for (int x = minX; x <= maxX; ++x) {
                const float sampleX = worldLeft +
                    (static_cast<float>(x) + 0.5f) * pixelWorldWidth;
                const float deltaX = sampleX - particle.pos.x;
                const float distanceSquared =
                    deltaX * deltaX + deltaY * deltaY;
                if (distanceSquared >= particleRadiusSquared) continue;
                const float kernel =
                    1.0f - distanceSquared / particleRadiusSquared;
                const std::size_t index =
                    static_cast<std::size_t>(y) * fieldWidth +
                    static_cast<std::size_t>(x);
                output[index] = std::min(
                    1.0f, output[index] + peakDensity * kernel);
            }
        }
    }
    return true;
}

MpmBounds2D MpmSolver2D::particleBounds(float padding) const noexcept {
    MpmBounds2D bounds;
    if (!std::isfinite(padding) || padding < 0.0f) return bounds;
    if (!particleBoundsCacheValid_) refreshParticleBoundsCache();
    if (!particleBoundsCache_.valid) return bounds;

    const float left = particleBoundsCache_.left - padding;
    const float top = particleBoundsCache_.top - padding;
    const float right = particleBoundsCache_.right + padding;
    const float bottom = particleBoundsCache_.bottom + padding;
    if (!std::isfinite(left) || !std::isfinite(top) ||
        !std::isfinite(right) || !std::isfinite(bottom) ||
        right < left || bottom < top) {
        return bounds;
    }
    bounds.left = left;
    bounds.top = top;
    bounds.right = right;
    bounds.bottom = bottom;
    bounds.valid = true;
    return bounds;
}

void MpmSolver2D::refreshParticleBoundsCache() const noexcept {
    MpmBounds2D bounds;
    float minimumX = std::numeric_limits<float>::max();
    float minimumY = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float maximumY = std::numeric_limits<float>::lowest();
    for (const auto& particle : particles_) {
        if (!particle.active || !std::isfinite(particle.pos.x) ||
            !std::isfinite(particle.pos.y)) {
            continue;
        }
        minimumX = std::min(minimumX, particle.pos.x);
        minimumY = std::min(minimumY, particle.pos.y);
        maximumX = std::max(maximumX, particle.pos.x);
        maximumY = std::max(maximumY, particle.pos.y);
        bounds.valid = true;
    }
    if (bounds.valid) {
        bounds.left = minimumX;
        bounds.top = minimumY;
        bounds.right = maximumX;
        bounds.bottom = maximumY;
    }
    particleBoundsCache_ = bounds;
    particleBoundsCacheValid_ = true;
}

bool MpmSolver2D::expandGranularGridForParticles() noexcept {
    if (constitutiveModel_ !=
        MpmConstitutiveModel::GranularDruckerPrager) {
        return true;
    }
    const MpmBounds2D bounds = particleBounds();
    if (!bounds.valid) return true;

    constexpr double kSafeMarginCells = 2.0;
    constexpr double kExpansionMarginCells = 4.0;
    const double cellSize = cellSize_;
    const double currentLeft = gridOrigin_.x;
    const double currentTop = gridOrigin_.y;
    const double currentRight = currentLeft +
        static_cast<double>(nx_) * cellSize;
    const double currentBottom = currentTop +
        static_cast<double>(ny_) * cellSize;
    const double safeLeft = currentLeft + kSafeMarginCells * cellSize;
    const double safeTop = currentTop + kSafeMarginCells * cellSize;
    const double safeRight = currentRight - kSafeMarginCells * cellSize;
    const double safeBottom = currentBottom - kSafeMarginCells * cellSize;
    if (bounds.left >= safeLeft && bounds.top >= safeTop &&
        bounds.right <= safeRight && bounds.bottom <= safeBottom) {
        return true;
    }

    const double desiredLeft = std::min(
        currentLeft, static_cast<double>(bounds.left) -
                         kExpansionMarginCells * cellSize);
    const double desiredTop = std::min(
        currentTop, static_cast<double>(bounds.top) -
                        kExpansionMarginCells * cellSize);
    const double desiredRight = std::max(
        currentRight, static_cast<double>(bounds.right) +
                          kExpansionMarginCells * cellSize);
    const double desiredBottom = std::max(
        currentBottom, static_cast<double>(bounds.bottom) +
                           kExpansionMarginCells * cellSize);
    const double alignedLeft = currentLeft - std::ceil(
        (currentLeft - desiredLeft) / cellSize) * cellSize;
    const double alignedTop = currentTop - std::ceil(
        (currentTop - desiredTop) / cellSize) * cellSize;
    const double requestedColumns = std::ceil(
        (desiredRight - alignedLeft) / cellSize);
    const double requestedRows = std::ceil(
        (desiredBottom - alignedTop) / cellSize);
    const double maxFloat = std::numeric_limits<float>::max();
    if (!std::isfinite(alignedLeft) || !std::isfinite(alignedTop) ||
        !std::isfinite(requestedColumns) ||
        !std::isfinite(requestedRows) || requestedColumns < nx_ ||
        requestedRows < ny_ ||
        requestedColumns > std::numeric_limits<int>::max() ||
        requestedRows > std::numeric_limits<int>::max() ||
        std::abs(alignedLeft) > maxFloat || std::abs(alignedTop) > maxFloat) {
        gridExpansionLimitReached_ = true;
        return false;
    }
    const auto newColumns = static_cast<std::size_t>(requestedColumns);
    const auto newRows = static_cast<std::size_t>(requestedRows);
    if (newRows == 0 || newColumns > grid_.size() / newRows) {
        gridExpansionLimitReached_ = true;
        return false;
    }
    const float newOriginX = static_cast<float>(alignedLeft);
    const float newOriginY = static_cast<float>(alignedTop);
    const double newRight = alignedLeft + requestedColumns * cellSize;
    const double newBottom = alignedTop + requestedRows * cellSize;
    if (!std::isfinite(newOriginX) || !std::isfinite(newOriginY) ||
        !std::isfinite(newRight) || !std::isfinite(newBottom) ||
        std::abs(newRight) > maxFloat || std::abs(newBottom) > maxFloat) {
        gridExpansionLimitReached_ = true;
        return false;
    }

    nx_ = static_cast<int>(newColumns);
    ny_ = static_cast<int>(newRows);
    gridOrigin_ = {newOriginX, newOriginY};
    return true;
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
    MpmSnapshot2D result;
    snapshot(result);
    return result;
}

void MpmSolver2D::snapshot(MpmSnapshot2D& destination) const {
    // Vector assignment preserves destination capacity, allowing checkpoint
    // storage to recycle its bounded particle and fracture-index buffers.
    destination.particles = particles_;
    destination.fracturedIndices = fracturedIndices_;
    destination.gridOrigin = gridOrigin_;
    destination.accumulatedTime = accumulatedTime_;
    destination.particleGridColumns = particleGridColumns_;
    destination.particleGridRows = particleGridRows_;
    destination.solverGridCellSize = cellSize_;
    destination.solverGridColumns = nx_;
    destination.solverGridRows = ny_;
    destination.gridExpansionLimitReached = gridExpansionLimitReached_;
}

bool MpmSolver2D::canRestoreSnapshot(const MpmSnapshot2D& snapshot) const noexcept {
    constexpr float kMaximumSnapshotAccumulator = 0.1f * 1024.0f;
    constexpr float kAccumulatorRoundoffTolerance = 1.0e-7f;
    if (snapshot.particleGridColumns < 0 || snapshot.particleGridRows < 0) {
        return false;
    }
    if ((snapshot.particleGridColumns == 0) !=
        (snapshot.particleGridRows == 0)) {
        return false;
    }
    if (snapshot.particleGridColumns == 0 &&
        (!snapshot.particles.empty() ||
         !snapshot.fracturedIndices.empty())) {
        return false;
    }
    if (snapshot.particleGridColumns > 0) {
        const std::size_t columns =
            static_cast<std::size_t>(snapshot.particleGridColumns);
        const std::size_t rows =
            static_cast<std::size_t>(snapshot.particleGridRows);
        if (columns > std::numeric_limits<std::size_t>::max() / rows ||
            snapshot.particles.size() != columns * rows) {
            return false;
        }
    }
    if (!std::isfinite(snapshot.gridOrigin.x) ||
        !std::isfinite(snapshot.gridOrigin.y) ||
        !std::isfinite(snapshot.solverGridCellSize) ||
        snapshot.solverGridCellSize <= 0.0f ||
        snapshot.solverGridColumns < 2 || snapshot.solverGridRows < 2 ||
        !std::isfinite(snapshot.accumulatedTime) ||
        snapshot.accumulatedTime < -kAccumulatorRoundoffTolerance ||
        snapshot.accumulatedTime > kMaximumSnapshotAccumulator ||
        !std::isfinite(snapshot.physicsWorldAccumulator) ||
        snapshot.physicsWorldAccumulator < 0.0f ||
        snapshot.physicsWorldAccumulator > kMaximumSnapshotAccumulator) {
        return false;
    }
    const std::size_t snapshotGridColumns = static_cast<std::size_t>(
        snapshot.solverGridColumns);
    const std::size_t snapshotGridRows = static_cast<std::size_t>(
        snapshot.solverGridRows);
    if (snapshotGridRows == 0 ||
        snapshotGridColumns > grid_.size() / snapshotGridRows ||
        snapshot.solverGridCellSize != cellSize_ ||
        snapshot.particleGridColumns != particleGridColumns_ ||
        snapshot.particleGridRows != particleGridRows_) {
        return false;
    }
    const double snapshotGridRight =
        static_cast<double>(snapshot.gridOrigin.x) +
        static_cast<double>(snapshot.solverGridColumns) *
            snapshot.solverGridCellSize;
    const double snapshotGridBottom =
        static_cast<double>(snapshot.gridOrigin.y) +
        static_cast<double>(snapshot.solverGridRows) *
            snapshot.solverGridCellSize;
    const double maxFloat = std::numeric_limits<float>::max();
    if (!std::isfinite(snapshotGridRight) ||
        !std::isfinite(snapshotGridBottom) ||
        std::abs(snapshotGridRight) > maxFloat ||
        std::abs(snapshotGridBottom) > maxFloat) {
        return false;
    }
    if (constitutiveModel_ ==
            MpmConstitutiveModel::GranularDruckerPrager &&
        (snapshot.particles.size() != particles_.size() ||
         !snapshot.fracturedIndices.empty())) {
        return false;
    }
    const auto finiteMatrix = [](const MpmMat2& matrix) noexcept {
        return std::isfinite(matrix.m00) && std::isfinite(matrix.m01) &&
               std::isfinite(matrix.m10) && std::isfinite(matrix.m11);
    };
    const float granularMarginX =
        snapshot.solverGridColumns >= 4 ? 2.0f : 0.5f;
    const float granularMarginY =
        snapshot.solverGridRows >= 4 ? 2.0f : 0.5f;
    const float granularMinX =
        snapshot.gridOrigin.x + granularMarginX * snapshot.solverGridCellSize;
    const float granularMinY =
        snapshot.gridOrigin.y + granularMarginY * snapshot.solverGridCellSize;
    const float granularMaxX = snapshot.gridOrigin.x +
        (static_cast<float>(snapshot.solverGridColumns) - granularMarginX) *
            snapshot.solverGridCellSize;
    const float granularMaxY = snapshot.gridOrigin.y +
        (static_cast<float>(snapshot.solverGridRows) - granularMarginY) *
            snapshot.solverGridCellSize;
    for (std::size_t particleIndex = 0;
         particleIndex < snapshot.particles.size(); ++particleIndex) {
        const auto& particle = snapshot.particles[particleIndex];
        if (constitutiveModel_ ==
            MpmConstitutiveModel::GranularDruckerPrager) {
            const auto& seededParticle = particles_[particleIndex];
            const float deformationDeterminant = particle.F.det();
            const float plasticDeterminant = particle.Fp.det();
            if (particle.mass != seededParticle.mass ||
                particle.volume0 != seededParticle.volume0 ||
                particle.active != seededParticle.active ||
                !particle.active || !std::isfinite(particle.pos.x) ||
                !std::isfinite(particle.pos.y) ||
                particle.pos.x < granularMinX ||
                particle.pos.x > granularMaxX ||
                particle.pos.y < granularMinY ||
                particle.pos.y > granularMaxY ||
                !std::isfinite(deformationDeterminant) ||
                deformationDeterminant <= 1.0e-12f ||
                !std::isfinite(plasticDeterminant) ||
                plasticDeterminant <= 1.0e-12f ||
                !std::isfinite(particle.plasticStrain) ||
                particle.plasticStrain < 0.0f ||
                particle.plasticStrain > 100.0f) {
                return false;
            }
        }
        if (!std::isfinite(particle.pos.x) ||
            !std::isfinite(particle.pos.y) ||
            !std::isfinite(particle.vel.x) ||
            !std::isfinite(particle.vel.y) ||
            !std::isfinite(particle.mass) || particle.mass < 0.0f ||
            !std::isfinite(particle.volume0) || particle.volume0 < 0.0f ||
            !std::isfinite(particle.plasticStrain) ||
            !std::isfinite(particle.r) || !std::isfinite(particle.g) ||
            !std::isfinite(particle.b) || !finiteMatrix(particle.F) ||
            !finiteMatrix(particle.Fp) || !finiteMatrix(particle.C) ||
            (particle.active &&
             (particle.mass <= 0.0f || particle.volume0 <= 0.0f))) {
            return false;
        }
    }
    for (const int fracturedIndex : snapshot.fracturedIndices) {
        if (fracturedIndex < 0 ||
            static_cast<std::size_t>(fracturedIndex) >=
                snapshot.particles.size()) {
            return false;
        }
    }
    return true;
}

bool MpmSolver2D::restoreSnapshot(const MpmSnapshot2D& snapshot) {
    if (!canRestoreSnapshot(snapshot)) return false;
    particles_ = snapshot.particles;
    particleBoundsCacheValid_ = false;
    fracturedIndices_ = snapshot.fracturedIndices;
    gridOrigin_ = snapshot.gridOrigin;
    nx_ = snapshot.solverGridColumns;
    ny_ = snapshot.solverGridRows;
    accumulatedTime_ = snapshot.accumulatedTime;
    particleGridColumns_ = snapshot.particleGridColumns;
    particleGridRows_ = snapshot.particleGridRows;
    gridExpansionLimitReached_ = snapshot.gridExpansionLimitReached;
    return true;
}

// ---- particle generation ----

MpmParticleGridSeedGeometry2D calculateMpmParticleGridSeedGeometry2D(
    float centerX, float centerY, float width, float height,
    int columns, int rows, float density, bool granularSand) noexcept {
    MpmParticleGridSeedGeometry2D geometry;
    if (!std::isfinite(centerX) || !std::isfinite(centerY) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(density) || width <= 0.0f || height <= 0.0f ||
        density <= 0.0f || columns <= 0 || rows <= 0) {
        return geometry;
    }

    const int safeColumns = std::max(1, columns);
    const int safeRows = std::max(1, rows);
    const std::size_t particleColumns =
        static_cast<std::size_t>(safeColumns);
    const std::size_t particleRows = static_cast<std::size_t>(safeRows);
    if (particleColumns >
        std::numeric_limits<std::size_t>::max() / particleRows) {
        return geometry;
    }
    geometry.particleCount = particleColumns * particleRows;
    const float columnDivisor = static_cast<float>(safeColumns);
    const float rowDivisor = static_cast<float>(safeRows);
    geometry.stepX = granularSand
        ? width / columnDivisor
        : (safeColumns > 1
               ? width / static_cast<float>(safeColumns - 1)
               : 0.0f);
    geometry.stepY = granularSand
        ? height / rowDivisor
        : (safeRows > 1
               ? height / static_cast<float>(safeRows - 1)
               : 0.0f);
    geometry.startX = centerX - width * 0.5f +
        (granularSand ? geometry.stepX * 0.5f : 0.0f);
    geometry.startY = centerY - height * 0.5f +
        (granularSand ? geometry.stepY * 0.5f : 0.0f);
    geometry.totalVolume = width * height * 1.0f;
    geometry.particleVolume = geometry.totalVolume /
        static_cast<float>(geometry.particleCount);
    geometry.particleMass = density * geometry.particleVolume;
    geometry.lastX = geometry.startX +
        static_cast<float>(safeColumns - 1) * geometry.stepX;
    geometry.lastY = geometry.startY +
        static_cast<float>(safeRows - 1) * geometry.stepY;
    geometry.valid =
        std::isfinite(geometry.stepX) && std::isfinite(geometry.stepY) &&
        std::isfinite(geometry.startX) && std::isfinite(geometry.startY) &&
        std::isfinite(geometry.lastX) && std::isfinite(geometry.lastY) &&
        std::isfinite(geometry.totalVolume) && geometry.totalVolume > 0.0f &&
        std::isfinite(geometry.particleVolume) &&
        geometry.particleVolume > 0.0f &&
        std::isfinite(geometry.particleMass) && geometry.particleMass > 0.0f &&
        (!granularSand ||
         (geometry.stepX > 0.0f && geometry.stepY > 0.0f));
    return geometry;
}

void MpmSolver2D::addParticlesGrid(float cx, float cy, float w, float h,
                                   int cols, int rows, float density) {
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(w) ||
        !std::isfinite(h) || !std::isfinite(density) || w <= 0.0f ||
        h <= 0.0f || density <= 0.0f || cols <= 0 || rows <= 0) {
        return;
    }
    int nc = std::max(1, cols);
    int nr = std::max(1, rows);
    const std::size_t particleColumns = static_cast<std::size_t>(nc);
    const std::size_t particleRows = static_cast<std::size_t>(nr);
    if (particleColumns > particles_.max_size() / particleRows) return;
    const std::size_t particleCount =
        particleColumns * particleRows;
    if (particleCount > particles_.max_size() - particles_.size()) return;
    const bool granularSand = constitutiveModel_ ==
        MpmConstitutiveModel::GranularDruckerPrager;
    const MpmParticleGridSeedGeometry2D geometry =
        calculateMpmParticleGridSeedGeometry2D(
            cx, cy, w, h, nc, nr, density, granularSand);
    if (!geometry.valid || geometry.particleCount != particleCount) return;

    particles_.reserve(particles_.size() + particleCount);
    particleGridColumns_ = nc;
    particleGridRows_ = nr;

    for (int r = 0; r < nr; ++r) {
        for (int c = 0; c < nc; ++c) {
            MpmParticle2D p;
            p.pos.x    = geometry.startX + static_cast<float>(c) * geometry.stepX;
            p.pos.y    = geometry.startY + static_cast<float>(r) * geometry.stepY;
            p.mass     = geometry.particleMass;
            p.volume0  = geometry.particleVolume;
            p.vel      = MpmVec2{};
            p.F        = MpmMat2::identity();
            p.Fp       = MpmMat2::identity();
            p.C        = MpmMat2::zero();
            p.active   = true;
            p.plasticStrain = 0.0f;

            // colour proportional to position
            p.r = 0.3f + 0.5f * (p.pos.x - geometry.startX) / (w + 1.0f);
            p.g = 0.3f + 0.5f * (p.pos.y - geometry.startY) / (h + 1.0f);
            p.b = 0.2f;
            particles_.push_back(p);
        }
    }
    particleBoundsCacheValid_ = false;
}

void MpmSolver2D::addParticlesRandom(float cx, float cy, float radius,
                                     int count, float density) {
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius) ||
        !std::isfinite(density) || radius <= 0.0f || density <= 0.0f || count <= 0) {
        return;
    }
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
    particleBoundsCacheValid_ = false;
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
                grid_[idx].vel +=
                    w * p.mass * (p.vel + apicContrib);
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

        const MpmMat2 FpInverse = p.Fp.inverse();
        MpmMat2 Fe = p.F * FpInverse;
        MpmMat2 P  = firstPiolaKirchhoff(Fe);
        if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
            // firstPiolaKirchhoff(Fe) differentiates the elastic energy with
            // respect to Fe. Grid forces are differentiated with respect to
            // total F, where Fe = F * Fp^{-1}; apply the chain rule
            // P_total = P_elastic * Fp^{-T}.
            P = P * FpInverse.transpose();
        }

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
    const int nodeCount = nx_ * ny_;
    // Granular CFL substeps vary with the state, so interpret its authored
    // damping as a rate per 1/120 s instead of damping once per substep.
    const float dampingFactor = damping_ > 0.0f
        ? (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager
               ? std::pow(1.0f - damping_, dt * 120.0f)
               : 1.0f - damping_)
        : 1.0f;
    Parallel::For(0, nodeCount, [&](int idx) {
            auto& node = grid_[idx];
            if (node.mass <= 0.0f) return;

            // v = (mv + dt * f_gravity + dt * f_internal) / m
            float invMass = 1.0f / node.mass;
            float gx = gravityX_;
            float gy = gravityY_;

            MpmVec2 accel{ gx + node.force.x * invMass,
                           gy + node.force.y * invMass };

            MpmVec2 vNew = (node.vel * invMass) + accel * dt;

            // damping (global)
            vNew *= dampingFactor;

            node.vel = vNew * node.mass; // store as momentum for G2P
    });
}

// ---- MPM step: G2P with APIC ----

void MpmSolver2D::gridToParticle(float dt) {
    float dx  = cellSize_;
    float inv = 1.0f / dx;
    float dScale = 4.0f / (dx * dx); // scaling for APIC C update
    const bool updateGranularDeformation = constitutiveModel_ ==
        MpmConstitutiveModel::GranularDruckerPrager;

    Parallel::ForSize(0, particles_.size(), [&](std::size_t particleIndex) {
        auto& p = particles_[particleIndex];
        if (!p.active) return;

        float fx = (p.pos.x - gridOrigin_.x) * inv;
        float fy = (p.pos.y - gridOrigin_.y) * inv;
        int ix = static_cast<int>(std::floor(fx));
        int iy = static_cast<int>(std::floor(fy));

        MpmVec2  newVel;
        MpmMat2  newC;  // zero initialised
        MpmMat2  gradV;

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

                if (updateGranularDeformation) {
                    const float dwx = dweight(rx, dx);
                    const float dwy = dweight(ry, dx);
                    const float gradW = dwx * weight(ry, dx);
                    const float gradH = weight(rx, dx) * dwy;
                    gradV.m00 += vNode.x * gradW;
                    gradV.m01 += vNode.x * gradH;
                    gradV.m10 += vNode.y * gradW;
                    gradV.m11 += vNode.y * gradH;
                }

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
        if (updateGranularDeformation) {
            // Sample the velocity gradient at the old particle position and
            // update F before moving the particle to its new position.
            const MpmMat2 dF{
                1.0f + dt * gradV.m00, dt * gradV.m01,
                dt * gradV.m10, 1.0f + dt * gradV.m11};
            p.F = dF * p.F;
        }
        p.pos.x += newVel.x * dt;
        p.pos.y += newVel.y * dt;
    });
}

// ---- MPM step: update deformation gradient ----

void MpmSolver2D::updateDeformationGradient(float dt) {
    float dx  = cellSize_;
    float inv = 1.0f / dx;

    Parallel::ForSize(0, particles_.size(), [&](std::size_t particleIndex) {
        auto& p = particles_[particleIndex];
        if (!p.active) return;

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
    });
}

// ---- von Mises plasticity with hardening ----
// Project the singular values of Fe = F * Fp^{-1}

void MpmSolver2D::applyPlasticity() {
    Parallel::ForSize(0, particles_.size(), [&](std::size_t particleIndex) {
        auto& p = particles_[particleIndex];
        if (!p.active) return;

        if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
            applyGranularPlasticity(p);
            return;
        }

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
    });
}

void MpmSolver2D::applyGranularPlasticity(MpmParticle2D& p) {
    const float plasticDeterminant = p.Fp.det();
    if (!std::isfinite(plasticDeterminant) ||
        plasticDeterminant <= 1.0e-12f) {
        return;
    }
    const MpmMat2 Fe = p.F * p.Fp.inverse();
    const MpmPositiveSvd2D svd = decomposePositiveMpm2D(Fe);
    if (!svd.valid || svd.singular0 <= 1.0e-8f ||
        svd.singular1 <= 1.0e-8f) {
        return;
    }

    const MpmHenckyProjection2D projection = projectMpmGranularHencky2D(
        std::log(svd.singular0), std::log(svd.singular1), lambda_, mu_,
        hardenedMpmFrictionAlpha2D(granularFrictionAngle_, p.plasticStrain));
    if (!projection.valid || !projection.projected) return;

    const float newS0 = std::exp(projection.logStretch0);
    const float newS1 = std::exp(projection.logStretch1);
    const float u1x = -svd.u0y, u1y = svd.u0x;
    const float v1x = -svd.v0y, v1y = svd.v0x;
    const MpmMat2 FeNew{
        newS0 * svd.u0x * svd.v0x + newS1 * u1x * v1x,
        newS0 * svd.u0x * svd.v0y + newS1 * u1x * v1y,
        newS0 * svd.u0y * svd.v0x + newS1 * u1y * v1x,
        newS0 * svd.u0y * svd.v0y + newS1 * u1y * v1y};
    if (!std::isfinite(FeNew.det()) || FeNew.det() <= 1.0e-12f) return;

    // F = Fe * Fp. Recompute the plastic factor from the projected elastic
    // state so the product remains the same total deformation gradient.
    const MpmMat2 FpNew = FeNew.inverse() * p.F;
    if (!std::isfinite(FpNew.det()) || FpNew.det() <= 0.0f) return;
    const MpmMat2 Fnew = FeNew * FpNew;
    const float plasticStrainNew = std::min(
        100.0f, p.plasticStrain + projection.plasticMultiplier);
    if (!std::isfinite(Fnew.m00) || !std::isfinite(Fnew.m01) ||
        !std::isfinite(Fnew.m10) || !std::isfinite(Fnew.m11) ||
        !std::isfinite(plasticStrainNew)) {
        return;
    }
    p.Fp = FpNew;
    p.F = Fnew;
    p.plasticStrain = plasticStrainNew;
}

// ---- fracture ----

void MpmSolver2D::checkFracture() {
    if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
        return;
    }
    if (!fractureEnabled_) return;
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
    if (!hasBoundary_ && !hasGranularFloor_) return;

    const int nodeCount = nx_ * ny_;
    Parallel::For(0, nodeCount, [&](int idx) {
            auto& node = grid_[idx];
            if (node.mass <= 0.0f) return;
            int i = idx % nx_;
            int j = idx / nx_;

            MpmVec2 pos{ gridOrigin_.x + (static_cast<float>(i) + 0.5f) * cellSize_,
                         gridOrigin_.y + (static_cast<float>(j) + 0.5f) * cellSize_ };

            MpmVec2 v = node.vel / node.mass;
            const bool granularSand = constitutiveModel_ ==
                MpmConstitutiveModel::GranularDruckerPrager;
            const auto applyGranularWallFriction =
                [this](float normalVelocityChange,
                       float& tangentVelocity) noexcept {
                    const float maximumTangentChange =
                        boundaryFriction_ * normalVelocityChange;
                    if (tangentVelocity > 0.0f) {
                        tangentVelocity = std::max(
                            0.0f, tangentVelocity - maximumTangentChange);
                    } else if (tangentVelocity < 0.0f) {
                        tangentVelocity = std::min(
                            0.0f, tangentVelocity + maximumTangentChange);
                    }
                };

            // left
            if (hasBoundary_ &&
                pos.x - cellSize_ * 0.5f <= bxmin_) {
                if (granularSand) {
                    if (v.x < 0.0f) {
                        const float normalVelocityChange = -v.x;
                        v.x = 0.0f;
                        applyGranularWallFriction(
                            normalVelocityChange, v.y);
                    }
                } else {
                    v.x = std::max(0.0f, v.x + boundaryFriction_ * v.x);
                    v.y *= (1.0f - boundaryFriction_);
                }
            }
            // right
            if (hasBoundary_ &&
                pos.x + cellSize_ * 0.5f >= bxmax_) {
                if (granularSand) {
                    if (v.x > 0.0f) {
                        const float normalVelocityChange = v.x;
                        v.x = 0.0f;
                        applyGranularWallFriction(
                            normalVelocityChange, v.y);
                    }
                } else {
                    v.x = std::min(0.0f, v.x - boundaryFriction_ * v.x);
                    v.y *= (1.0f - boundaryFriction_);
                }
            }
            // top (minimum y)
            if (hasBoundary_ &&
                pos.y - cellSize_ * 0.5f <= bymin_) {
                if (granularSand) {
                    if (v.y < 0.0f) {
                        const float normalVelocityChange = -v.y;
                        v.y = 0.0f;
                        applyGranularWallFriction(
                            normalVelocityChange, v.x);
                    }
                } else {
                    v.y = std::max(0.0f, v.y + boundaryFriction_ * v.y);
                    v.x *= (1.0f - boundaryFriction_);
                }
            }
            // bottom (maximum y)
            const float floorY = hasBoundary_ ? bymax_ : granularFloorY_;
            if ((hasBoundary_ || (granularSand && hasGranularFloor_)) &&
                pos.y + cellSize_ * 0.5f >= floorY) {
                if (granularSand) {
                    if (v.y > 0.0f) {
                        const float normalVelocityChange = v.y;
                        v.y = 0.0f;
                        applyGranularWallFriction(
                            normalVelocityChange, v.x);
                    }
                } else {
                    v.y = std::min(0.0f, v.y - boundaryFriction_ * v.y);
                    v.x *= (1.0f - boundaryFriction_);
                }
            }

            node.vel = v * node.mass;
    });
}

namespace {

void projectGranularAffineVelocityToTangent(
    MpmParticle2D& particle, float normalX, float normalY) noexcept {
    const float normalAffineX =
        normalX * particle.C.m00 + normalY * particle.C.m10;
    const float normalAffineY =
        normalX * particle.C.m01 + normalY * particle.C.m11;
    particle.C.m00 -= normalX * normalAffineX;
    particle.C.m01 -= normalX * normalAffineY;
    particle.C.m10 -= normalY * normalAffineX;
    particle.C.m11 -= normalY * normalAffineY;
}

void applyGranularColliderVelocityResponse(
    MpmParticle2D& particle, float normalX, float normalY,
    float friction, float restitution, bool granularSand) noexcept {
    const float normalVelocity =
        particle.vel.x * normalX + particle.vel.y * normalY;
    if (!granularSand) {
        if (normalVelocity < 0.0f) {
            particle.vel.x -=
                (1.0f + restitution) * normalVelocity * normalX;
            particle.vel.y -=
                (1.0f + restitution) * normalVelocity * normalY;
        }
        particle.vel.x *= 1.0f - friction;
        particle.vel.y *= 1.0f - friction;
        return;
    }
    if (normalVelocity >= 0.0f) return;

    const float normalVelocityChange =
        -(1.0f + restitution) * normalVelocity;
    particle.vel.x += normalVelocityChange * normalX;
    particle.vel.y += normalVelocityChange * normalY;

    projectGranularAffineVelocityToTangent(particle, normalX, normalY);
    const float tangentRetention = 1.0f - friction;
    particle.C.m00 *= tangentRetention;
    particle.C.m01 *= tangentRetention;
    particle.C.m10 *= tangentRetention;
    particle.C.m11 *= tangentRetention;

    const float postResponseNormalVelocity =
        particle.vel.x * normalX + particle.vel.y * normalY;
    const float tangentX =
        particle.vel.x - postResponseNormalVelocity * normalX;
    const float tangentY =
        particle.vel.y - postResponseNormalVelocity * normalY;
    const float tangentSpeed = std::hypot(tangentX, tangentY);
    const float maximumTangentChange = friction * normalVelocityChange;
    const float tangentScale = tangentSpeed > maximumTangentChange &&
            tangentSpeed > 0.0f
        ? (tangentSpeed - maximumTangentChange) / tangentSpeed
        : 0.0f;
    particle.vel.x = postResponseNormalVelocity * normalX +
                     tangentX * tangentScale;
    particle.vel.y = postResponseNormalVelocity * normalY +
                     tangentY * tangentScale;
}

struct MpmPolygonQueryResult {
    float closestX = 0.0f;
    float closestY = 0.0f;
    float contactNormalX = 0.0f;
    float contactNormalY = 0.0f;
    float firstVertexNormalX = 0.0f;
    float firstVertexNormalY = 0.0f;
    float secondVertexNormalX = 0.0f;
    float secondVertexNormalY = 0.0f;
    int closestVertexIndex = -1;
    bool inside = false;
};

MpmPolygonQueryResult mpmQueryPolygon(const std::vector<float>& poly,
                                      float px, float py,
                                      float polygonTwiceArea) {
    const int vertexCount = static_cast<int>(poly.size()) / 2;
    float bestDistSq = std::numeric_limits<float>::max();
    const float winding = polygonTwiceArea > 0.0f ? 1.0f
        : polygonTwiceArea < 0.0f ? -1.0f : 0.0f;
    MpmPolygonQueryResult result;
    result.closestX = px;
    result.closestY = py;
    for (int i = 0, j = vertexCount - 1; i < vertexCount; j = i++) {
        const float ax = poly[static_cast<std::size_t>(i * 2)];
        const float ay = poly[static_cast<std::size_t>(i * 2 + 1)];
        const float xj = poly[static_cast<std::size_t>(j * 2)];
        const float yj = poly[static_cast<std::size_t>(j * 2 + 1)];
        const float ex = xj - ax;
        const float ey = yj - ay;
        const float lenSq = ex * ex + ey * ey;
        float t = 0.0f;
        if (lenSq > 1e-12f) {
            t = std::clamp(((px - ax) * ex + (py - ay) * ey) / lenSq, 0.0f, 1.0f);
        }
        const float cx = ax + t * ex;
        const float cy = ay + t * ey;
        const int edgeVertexIndex = t <= 1.0e-5f ? i
            : t >= 1.0f - 1.0e-5f ? j : -1;
        const float dx = px - cx;
        const float dy = py - cy;
        const float distSq = dx * dx + dy * dy;
        const auto edgeUnitNormal = [ex, ey, winding](
                                       float& normalX,
                                       float& normalY) noexcept {
            const float edgeLength = std::hypot(ex, ey);
            if (edgeLength <= 1.0e-6f) return;
            normalX = winding * (-ey / edgeLength);
            normalY = winding * (ex / edgeLength);
        };
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            result.closestX = cx;
            result.closestY = cy;
            result.contactNormalX = 0.0f;
            result.contactNormalY = 0.0f;
            result.firstVertexNormalX = 0.0f;
            result.firstVertexNormalY = 0.0f;
            result.secondVertexNormalX = 0.0f;
            result.secondVertexNormalY = 0.0f;
            result.closestVertexIndex = edgeVertexIndex;
            edgeUnitNormal(result.contactNormalX, result.contactNormalY);
            result.firstVertexNormalX = result.contactNormalX;
            result.firstVertexNormalY = result.contactNormalY;
        } else if (std::abs(distSq - bestDistSq) <= 1.0e-12f &&
                   std::abs(cx - result.closestX) <= 1.0e-5f &&
                   std::abs(cy - result.closestY) <= 1.0e-5f) {
            float normalX = 0.0f;
            float normalY = 0.0f;
            edgeUnitNormal(normalX, normalY);
            result.contactNormalX += normalX;
            result.contactNormalY += normalY;
            if (edgeVertexIndex >= 0 &&
                (result.closestVertexIndex < 0 ||
                 result.closestVertexIndex == edgeVertexIndex)) {
                result.closestVertexIndex = edgeVertexIndex;
                result.secondVertexNormalX = normalX;
                result.secondVertexNormalY = normalY;
            }
        }
        if ((ay > py) != (yj > py)) {
            const float t = (py - ay) / (yj - ay);
            if (px < ax + t * (xj - ax)) result.inside = !result.inside;
        }
    }
    return result;
}

void resolvePolygonCollider(MpmParticle2D& particle, const MpmCollider2D& collider,
                            float friction, float restitution,
                            bool granularSand) {
    const int vertexCount =
        static_cast<int>(collider.polygonPoints.size()) / 2;
    if (vertexCount < 3) return;
    if (granularSand &&
        std::abs(collider.polygonTwiceArea) <= 1.0e-12f) {
        return;
    }
    const MpmPolygonQueryResult query = mpmQueryPolygon(
        collider.polygonPoints, particle.pos.x, particle.pos.y,
        collider.polygonTwiceArea);
    const float nx = particle.pos.x - query.closestX;
    const float ny = particle.pos.y - query.closestY;
    const float normalLengthSq = nx * nx + ny * ny;
    constexpr float kBoundaryDistanceSq = 1.0e-10f;
    const bool onGranularBoundary =
        granularSand && normalLengthSq <= kBoundaryDistanceSq;
    if (!query.inside && !onGranularBoundary) return;

    particle.pos.x = query.closestX;
    particle.pos.y = query.closestY;
    if (normalLengthSq > 1e-12f) {
        const float invNormalLength = 1.0f / std::sqrt(normalLengthSq);
        const float outwardSign = granularSand ? -1.0f : 1.0f;
        const float ux = outwardSign * nx * invNormalLength;
        const float uy = outwardSign * ny * invNormalLength;
        applyGranularColliderVelocityResponse(
            particle, ux, uy, friction, restitution, granularSand);
    } else if (granularSand) {
        float normalX = query.contactNormalX;
        float normalY = query.contactNormalY;
        if (query.closestVertexIndex >= 0 &&
            std::hypot(query.secondVertexNormalX,
                       query.secondVertexNormalY) > 1.0e-12f) {
            const float firstApproach =
                particle.vel.x * query.firstVertexNormalX +
                particle.vel.y * query.firstVertexNormalY;
            const float secondApproach =
                particle.vel.x * query.secondVertexNormalX +
                particle.vel.y * query.secondVertexNormalY;
            const int previousIndex =
                (query.closestVertexIndex + vertexCount - 1) % vertexCount;
            const int nextIndex = (query.closestVertexIndex + 1) % vertexCount;
            const float vertexX = collider.polygonPoints[
                static_cast<std::size_t>(query.closestVertexIndex * 2)];
            const float vertexY = collider.polygonPoints[
                static_cast<std::size_t>(query.closestVertexIndex * 2 + 1)];
            const float incomingX = vertexX - collider.polygonPoints[
                static_cast<std::size_t>(previousIndex * 2)];
            const float incomingY = vertexY - collider.polygonPoints[
                static_cast<std::size_t>(previousIndex * 2 + 1)];
            const float outgoingX = collider.polygonPoints[
                static_cast<std::size_t>(nextIndex * 2)] - vertexX;
            const float outgoingY = collider.polygonPoints[
                static_cast<std::size_t>(nextIndex * 2 + 1)] - vertexY;
            const float turn = (incomingX * outgoingY -
                                incomingY * outgoingX) *
                               (collider.polygonTwiceArea > 0.0f
                                    ? 1.0f : -1.0f);
            const bool reflexVertex = turn < -1.0e-6f;
            if (reflexVertex || firstApproach >= 0.0f ||
                secondApproach >= 0.0f) {
                if (firstApproach < secondApproach) {
                    normalX = query.firstVertexNormalX;
                    normalY = query.firstVertexNormalY;
                } else {
                    normalX = query.secondVertexNormalX;
                    normalY = query.secondVertexNormalY;
                }
            }
        }
        const float normalLength = std::hypot(normalX, normalY);
        if (normalLength > 1.0e-12f) {
            const float ux = normalX / normalLength;
            const float uy = normalY / normalLength;
            applyGranularColliderVelocityResponse(
                particle, ux, uy, friction, restitution, granularSand);
        }
    }
}

} // namespace

void MpmSolver2D::resolveColliders() {
    const bool granularSand = constitutiveModel_ ==
        MpmConstitutiveModel::GranularDruckerPrager;
    for (auto& particle : particles_) {
        if (!particle.active) continue;
        for (const auto& collider : colliders_) {
            if (!collider.enabled) continue;
            const float friction = granularSand
                ? collider.granularFriction : collider.clampedFriction;
            const float restitution = granularSand
                ? collider.granularRestitution : collider.clampedRestitution;
            if (collider.type == MpmCollider2D::Type::Plane) {
                if (granularSand) {
                    // Granular sand uses Composition screen coordinates:
                    // +Y is downward, so a horizontal plane is a floor and
                    // particles crossing below it must respond along -Y.
                    if (particle.pos.y > collider.y) {
                        particle.pos.y = collider.y;
                        applyGranularColliderVelocityResponse(
                            particle, 0.0f, -1.0f, friction, restitution, true);
                    }
                    continue;
                }
                if (particle.pos.y < collider.y) {
                    particle.pos.y = collider.y;
                    particle.vel.y =
                        std::abs(particle.vel.y) * restitution;
                    particle.vel.x *= 1.0f - friction;
                }
                continue;
            }
            if (collider.type == MpmCollider2D::Type::Circle) {
                const float dx = particle.pos.x - collider.x;
                const float dy = particle.pos.y - collider.y;
                const float radius = std::max(0.0f, collider.radius);
                const float distanceSq = dx * dx + dy * dy;
                if (distanceSq >= radius * radius) continue;
                float nx = 1.0f;
                float ny = 0.0f;
                if (distanceSq > 1e-8f) {
                    const float distance = std::sqrt(distanceSq);
                    nx = dx / distance;
                    ny = dy / distance;
                } else if (!granularSand) {
                    continue;
                }
                particle.pos.x = collider.x + nx * radius;
                particle.pos.y = collider.y + ny * radius;
                applyGranularColliderVelocityResponse(
                    particle, nx, ny, friction, restitution, granularSand);
                continue;
            }
            if (collider.type == MpmCollider2D::Type::Polygon) {
                // Preserve the granular on-boundary epsilon while rejecting
                // particles too far away to be inside or in contact.
                constexpr float kGranularBoundaryTolerance = 1.0e-5f;
                const float boundsTolerance = granularSand
                    ? kGranularBoundaryTolerance : 0.0f;
                if (particle.pos.x < collider.polygonMinX - boundsTolerance ||
                    particle.pos.x > collider.polygonMaxX + boundsTolerance) {
                    continue;
                }
                if (particle.pos.y < collider.polygonMinY - boundsTolerance ||
                    particle.pos.y > collider.polygonMaxY + boundsTolerance) {
                    continue;
                }
                resolvePolygonCollider(particle, collider, friction,
                                       restitution, granularSand);
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
                const bool hitLeft = minimum == left;
                particle.pos.x = hitLeft ? minX : maxX;
                if (granularSand) {
                    applyGranularColliderVelocityResponse(
                        particle, hitLeft ? -1.0f : 1.0f, 0.0f,
                        friction, restitution, true);
                } else {
                    particle.vel.x = (hitLeft ? -1.0f : 1.0f) *
                                     std::abs(particle.vel.x) * restitution;
                    particle.vel.y *= 1.0f - friction;
                }
            } else {
                const bool hitBottom = minimum == bottom;
                particle.pos.y = hitBottom ? minY : maxY;
                if (granularSand) {
                    applyGranularColliderVelocityResponse(
                        particle, 0.0f, hitBottom ? -1.0f : 1.0f,
                        friction, restitution, true);
                } else {
                    particle.vel.y = (hitBottom ? -1.0f : 1.0f) *
                                     std::abs(particle.vel.y) * restitution;
                    particle.vel.x *= 1.0f - friction;
                }
            }
        }
    }
}

// ---- impulse / body force ----

void MpmSolver2D::addImpulse(float x, float y, float force, float radius) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(force)) return;
    const float safeRadius = std::max(radius, 1e-4f);
    const float rSq = safeRadius * safeRadius;
    for (auto& p : particles_) {
        if (!p.active) continue;
        float dx = p.pos.x - x;
        float dy = p.pos.y - y;
        const float dSq = dx * dx + dy * dy;
        // Radial push away from the impulse center: direction follows the
        // actual particle offset and strength fades to zero at the rim.
        if (dSq > rSq || dSq <= 1e-12f) continue;
        const float falloff = 1.0f - dSq / rSq;
        const float invDist = 1.0f / std::sqrt(dSq);
        p.vel.x += force * falloff * dx * invDist;
        p.vel.y += force * falloff * dy * invDist;
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

MpmSolver2D::~MpmSolver2D() {
    detachGPUSimulation();
}

bool MpmSolver2D::setBackend(MpmBackend backend) noexcept {
    if (backend == MpmBackend::CPU) {
        backend_ = MpmBackend::CPU;
        return true;
    }
    if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
        return false;
    }
    // GPU requires an attached session; otherwise stay on the CPU lane.
    if (backend_ == MpmBackend::GPU && gpuSimulation_) return true;
    if (!gpuSimulation_) return false;
    backend_ = MpmBackend::GPU;
    return true;
}

bool MpmSolver2D::attachGPUSimulation(void* sharedRenderDevice,
                                      void* immediateContext) {
    auto* device = static_cast<Diligent::IRenderDevice*>(sharedRenderDevice);
    auto* context = static_cast<Diligent::IDeviceContext*>(immediateContext);
    if (!device || !context) return false;
    detachGPUSimulation();
    auto session = std::make_unique<MpmGpuSession>();
    session->device = device;
    session->context = context;
    gpuSimulation_ = session.release();
    return true;
}

void MpmSolver2D::detachGPUSimulation() {
    if (gpuSimulation_) {
        delete static_cast<MpmGpuSession*>(gpuSimulation_);
        gpuSimulation_ = nullptr;
    }
    backend_ = MpmBackend::CPU;
}

bool MpmSolver2D::isGPUReady() const noexcept {
    auto* session = static_cast<const MpmGpuSession*>(gpuSimulation_);
    if (!session || session->failed || !session->device || !session->context) return false;
    return session->initialized && session->compute && session->compute->ready();
}

// GPU lane: fixed-substep loop runs entirely in compute shaders
// (elastic only), then plasticity/fracture/colliders run once on the CPU
// with the readback state. Diverges from the CPU cadence by design until
// parity is demonstrated; every failure falls back to the CPU lane.
bool MpmSolver2D::updateGPUSubsteps(float elapsedSeconds) {
    if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
        return false;
    }
    auto* session = static_cast<MpmGpuSession*>(gpuSimulation_);
    if (!session || session->failed || !session->device || !session->context) {
        return false;
    }
    if (nx_ < 2 || ny_ < 2) return false;

    const float maxAccumulated = fixedDt_ * static_cast<float>(maxSubsteps_);
    accumulatedTime_ = std::min(accumulatedTime_ +
                                    std::min(elapsedSeconds, maxAccumulated),
                                maxAccumulated);
    int steps = static_cast<int>((accumulatedTime_ + 1e-8f) / fixedDt_);
    if (steps <= 0) return true;
    steps = std::min(steps, maxSubsteps_);

    const std::uint32_t needed =
        static_cast<std::uint32_t>(particles_.size());

    if (!session->initialized || session->capacity < needed ||
        session->gridNx != static_cast<std::uint32_t>(nx_) ||
        session->gridNy != static_cast<std::uint32_t>(ny_)) {
        session->compute.reset();
        session->gpuContext.reset();
        session->capacity = std::max<std::uint32_t>(needed, 4096u);
        session->gpuContext =
            std::make_unique<GpuContext>(session->device, session->context);
        session->compute =
            std::make_unique<MpmGPUCompute>(*session->gpuContext);
        if (!session->compute->initialize(static_cast<std::uint32_t>(nx_),
                                          static_cast<std::uint32_t>(ny_),
                                          session->capacity)) {
            session->failed = true;
            session->compute.reset();
            session->gpuContext.reset();
            return false;
        }
        session->initialized = true;
        session->gridNx = static_cast<std::uint32_t>(nx_);
        session->gridNy = static_cast<std::uint32_t>(ny_);
    }

    if (needed == 0) {
        accumulatedTime_ = std::max(
            0.0f, accumulatedTime_ - fixedDt_ * static_cast<float>(steps));
        return true;
    }

    std::vector<MpmGpuParticle> packed(needed);
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        const auto& p = particles_[i];
        auto& g = packed[i];
        g.posX = p.pos.x;
        g.posY = p.pos.y;
        g.velX = p.vel.x;
        g.velY = p.vel.y;
        g.mass = p.mass;
        g.volume0 = p.volume0;
        g.F00 = p.F.m00;
        g.F01 = p.F.m01;
        g.F10 = p.F.m10;
        g.F11 = p.F.m11;
        g.Fp00 = p.Fp.m00;
        g.Fp01 = p.Fp.m01;
        g.Fp10 = p.Fp.m10;
        g.Fp11 = p.Fp.m11;
        g.C00 = p.C.m00;
        g.C01 = p.C.m01;
        g.C10 = p.C.m10;
        g.C11 = p.C.m11;
        g.plasticStrain = p.plasticStrain;
        g.active = p.active ? 1.0f : 0.0f;
    }

    auto* deviceContext = session->context.RawPtr();
    session->compute->uploadParticles(deviceContext, packed.data(), needed);

    MpmGpuParams params;
    params.particleCount = needed;
    params.nodeCount = static_cast<std::uint32_t>(nx_) *
                       static_cast<std::uint32_t>(ny_);
    params.gridNx = static_cast<std::uint32_t>(nx_);
    params.gridNy = static_cast<std::uint32_t>(ny_);
    params.cellSize = cellSize_;
    params.originX = gridOrigin_.x;
    params.originY = gridOrigin_.y;
    params.invCellSize = 1.0f / cellSize_;
    params.dt = fixedDt_;
    params.gravityX = gravityX_;
    params.gravityY = gravityY_;
    params.mu = mu_;
    params.lambda = lambda_;
    params.damping = damping_;
    params.boundaryFriction = boundaryFriction_;
    params.bxmin = bxmin_;
    params.bxmax = bxmax_;
    params.bymin = bymin_;
    params.bymax = bymax_;
    params.hasBoundary = hasBoundary_ ? 1.0f : 0.0f;

    session->compute->simulateSubsteps(deviceContext, steps, params);
    session->compute->readbackParticles(deviceContext, packed.data(), needed);

    for (std::size_t i = 0; i < particles_.size(); ++i) {
        auto& p = particles_[i];
        const auto& g = packed[i];
        p.pos.x = g.posX;
        p.pos.y = g.posY;
        p.vel.x = g.velX;
        p.vel.y = g.velY;
        p.F = MpmMat2(g.F00, g.F01, g.F10, g.F11);
        p.C = MpmMat2(g.C00, g.C01, g.C10, g.C11);
    }

    // CPU tail once per frame: mirrors stepOnce order after the F update.
    applyPlasticity();
    resolveColliders();
    checkFracture();

    // particle-level boundary
    if (hasBoundary_ ||
        (constitutiveModel_ ==
             MpmConstitutiveModel::GranularDruckerPrager &&
         hasGranularFloor_)) {
        const bool granularSand = constitutiveModel_ ==
            MpmConstitutiveModel::GranularDruckerPrager;
        const float floorY = hasBoundary_ ? bymax_ : granularFloorY_;
        const float cellMargin = granularSand
            ? std::min(cellSize_ * 0.5f, granularSupportRadiusLimit_)
            : cellSize_ * 0.5f;
        constexpr float kTwoOverPi = 0.63661977236758134308f;
        for (auto& p : particles_) {
            if (!p.active) continue;
            float margin = cellMargin;
            if (granularSand && std::isfinite(p.volume0) &&
                p.volume0 > 0.0f) {
                const float deformationDeterminant = p.F.det();
                if (std::isfinite(deformationDeterminant) &&
                    deformationDeterminant > 0.0f) {
                    const float currentVolume =
                        p.volume0 * deformationDeterminant;
                    if (std::isfinite(currentVolume) && currentVolume > 0.0f) {
                        const float supportRadius =
                            1.25f * std::sqrt(currentVolume * kTwoOverPi);
                        if (std::isfinite(supportRadius) &&
                            supportRadius > 0.0f) {
                            margin = std::min({cellMargin, supportRadius,
                                               granularSupportRadiusLimit_});
                        }
                    }
                }
            }
            if (hasBoundary_ && p.pos.x < bxmin_ + margin) {
                p.pos.x = bxmin_ + margin;
                p.vel.x = 0.0f;
            }
            if (hasBoundary_ && p.pos.x > bxmax_ - margin) {
                p.pos.x = bxmax_ - margin;
                p.vel.x = 0.0f;
            }
            if (hasBoundary_ && p.pos.y < bymin_ + margin) {
                p.pos.y = bymin_ + margin;
                p.vel.y = 0.0f;
            }
            if ((hasBoundary_ || (granularSand && hasGranularFloor_)) &&
                p.pos.y > floorY - margin) {
                p.pos.y = floorY - margin;
                p.vel.y = 0.0f;
            }
        }
    }

    float boundsMinimumX = std::numeric_limits<float>::max();
    float boundsMinimumY = std::numeric_limits<float>::max();
    float boundsMaximumX = std::numeric_limits<float>::lowest();
    float boundsMaximumY = std::numeric_limits<float>::lowest();
    bool hasFiniteParticleBounds = false;
    for (auto& p : particles_) {
        if (!std::isfinite(p.pos.x) || !std::isfinite(p.pos.y) ||
            !std::isfinite(p.vel.x) || !std::isfinite(p.vel.y)) {
            p.pos.x = std::isfinite(p.pos.x) ? p.pos.x : 0.0f;
            p.pos.y = std::isfinite(p.pos.y) ? p.pos.y : 0.0f;
            p.vel.x = 0.0f; p.vel.y = 0.0f;
        }
        if (p.active && std::isfinite(p.pos.x) && std::isfinite(p.pos.y)) {
            boundsMinimumX = std::min(boundsMinimumX, p.pos.x);
            boundsMinimumY = std::min(boundsMinimumY, p.pos.y);
            boundsMaximumX = std::max(boundsMaximumX, p.pos.x);
            boundsMaximumY = std::max(boundsMaximumY, p.pos.y);
            hasFiniteParticleBounds = true;
        }
    }
    particleBoundsCache_ = hasFiniteParticleBounds
        ? MpmBounds2D{boundsMinimumX, boundsMinimumY,
                      boundsMaximumX, boundsMaximumY, true}
        : MpmBounds2D{};
    particleBoundsCacheValid_ = true;

    accumulatedTime_ = std::max(
        0.0f, accumulatedTime_ - fixedDt_ * static_cast<float>(steps));
    return true;
}

void MpmSolver2D::update(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) return;
    const bool granularSand = constitutiveModel_ ==
        MpmConstitutiveModel::GranularDruckerPrager;
    if (granularSand) {
        updateGranularTimeStepForState();
    }
    if (backend_ == MpmBackend::GPU && gpuSimulation_ &&
        updateGPUSubsteps(elapsedSeconds)) {
        return;
    }
    const float maxAccumulated = fixedDt_ * static_cast<float>(maxSubsteps_);
    if (constitutiveModel_ == MpmConstitutiveModel::GranularDruckerPrager) {
        // Preserve granular simulation time when the state-dependent CFL step
        // needs more than this call's bounded substep budget. The backlog is
        // serialized in MpmSnapshot2D; time beyond its safety bound is dropped.
        constexpr float kMaximumGranularSolverBacklog = 102.4f;
        accumulatedTime_ = std::min(accumulatedTime_ + elapsedSeconds,
                                    kMaximumGranularSolverBacklog);
    } else {
        accumulatedTime_ = std::min(
            accumulatedTime_ + std::min(elapsedSeconds, maxAccumulated),
            maxAccumulated);
    }
    int substeps = 0;
    while (substeps < maxSubsteps_) {
        // Stress and advection can raise the characteristic speed during a
        // batch. Recompute the granular CFL step before each subsequent
        // internal step, and defer work if the remaining elapsed time is
        // shorter than the newly selected step.
        if (granularSand && substeps > 0) {
            updateGranularTimeStepForState();
        }
        if (accumulatedTime_ + 1.0e-8f < fixedDt_) break;
        stepOnce(fixedDt_);
        accumulatedTime_ = std::max(0.0f, accumulatedTime_ - fixedDt_);
        ++substeps;
    }
}

void MpmSolver2D::updateGranularTimeStepForState() noexcept {
    constexpr double kCfl = 0.05;
    constexpr double kMaximumTimeStep = 1.0 / 120.0;
    constexpr double kMinimumTimeStep = 1.0e-6;
    constexpr double kMaximumStencilRadiusInCells =
        2.1213203435596425732; // 1.5 * sqrt(2), quadratic kernel support.
    const bool gridContainsParticles = expandGranularGridForParticles();
    double maximumWaveSpeedSquared = 0.0;
    double maximumParticleAdvectionSpeed = 0.0;
    const auto finiteMatrix = [](const MpmMat2& matrix) noexcept {
        return std::isfinite(matrix.m00) && std::isfinite(matrix.m01) &&
               std::isfinite(matrix.m10) && std::isfinite(matrix.m11);
    };
    const float domainCenterX = gridOrigin_.x +
        0.5f * static_cast<float>(nx_) * cellSize_;
    const float domainCenterY = gridOrigin_.y +
        0.5f * static_cast<float>(ny_) * cellSize_;
    // Keep repaired coordinates inside the grid's world extent before P2G
    // converts normalized positions to integer node indices.
    const float safeCenterX =
        std::isfinite(domainCenterX) ? domainCenterX : 0.0f;
    const float safeCenterY =
        std::isfinite(domainCenterY) ? domainCenterY : 0.0f;
    const float supportMarginX = nx_ >= 4 ? 2.0f : 0.5f;
    const float supportMarginY = ny_ >= 4 ? 2.0f : 0.5f;
    const float positionMinX =
        gridOrigin_.x + supportMarginX * cellSize_;
    const float positionMinY =
        gridOrigin_.y + supportMarginY * cellSize_;
    const float positionMaxX = gridOrigin_.x +
        (static_cast<float>(nx_) - supportMarginX) * cellSize_;
    const float positionMaxY = gridOrigin_.y +
        (static_cast<float>(ny_) - supportMarginY) * cellSize_;
    const bool hasSafePositionRange =
        std::isfinite(positionMinX) && std::isfinite(positionMinY) &&
        std::isfinite(positionMaxX) && std::isfinite(positionMaxY) &&
        positionMinX <= positionMaxX && positionMinY <= positionMaxY;
    for (auto& particle : particles_) {
        if (!particle.active) continue;
        const float previousPositionX = particle.pos.x;
        const float previousPositionY = particle.pos.y;
        if (!std::isfinite(particle.pos.x)) particle.pos.x = safeCenterX;
        if (!std::isfinite(particle.pos.y)) particle.pos.y = safeCenterY;
        if (hasSafePositionRange) {
            if (!gridContainsParticles) {
                if (particle.pos.x < positionMinX) {
                    particle.pos.x = positionMinX;
                    if (particle.vel.x < 0.0f) particle.vel.x = 0.0f;
                    projectGranularAffineVelocityToTangent(
                        particle, -1.0f, 0.0f);
                } else if (particle.pos.x > positionMaxX) {
                    particle.pos.x = positionMaxX;
                    if (particle.vel.x > 0.0f) particle.vel.x = 0.0f;
                    projectGranularAffineVelocityToTangent(
                        particle, 1.0f, 0.0f);
                }
                if (particle.pos.y < positionMinY) {
                    particle.pos.y = positionMinY;
                    if (particle.vel.y < 0.0f) particle.vel.y = 0.0f;
                    projectGranularAffineVelocityToTangent(
                        particle, 0.0f, -1.0f);
                } else if (particle.pos.y > positionMaxY) {
                    particle.pos.y = positionMaxY;
                    if (particle.vel.y > 0.0f) particle.vel.y = 0.0f;
                    projectGranularAffineVelocityToTangent(
                        particle, 0.0f, 1.0f);
                }
            } else {
                particle.pos.x = std::clamp(
                    particle.pos.x, positionMinX, positionMaxX);
                particle.pos.y = std::clamp(
                    particle.pos.y, positionMinY, positionMaxY);
            }
        } else {
            particle.pos.x = safeCenterX;
            particle.pos.y = safeCenterY;
        }
        if (particle.pos.x != previousPositionX ||
            particle.pos.y != previousPositionY) {
            particleBoundsCacheValid_ = false;
        }
        if (!std::isfinite(particle.vel.x) ||
            !std::isfinite(particle.vel.y)) {
            particle.vel = MpmVec2{};
        }
        const float deformationDeterminant = particle.F.det();
        if (!finiteMatrix(particle.F) ||
            !std::isfinite(deformationDeterminant) ||
            deformationDeterminant <= 1.0e-12f) {
            particle.F = MpmMat2::identity();
            particle.C = MpmMat2::zero();
            particle.vel = MpmVec2{};
        }
        const float plasticDeterminant = particle.Fp.det();
        if (!finiteMatrix(particle.Fp) ||
            !std::isfinite(plasticDeterminant) ||
            plasticDeterminant <= 1.0e-12f) {
            particle.Fp = MpmMat2::identity();
            particle.C = MpmMat2::zero();
            particle.vel = MpmVec2{};
        }
        if (!finiteMatrix(particle.C)) particle.C = MpmMat2::zero();
        if (!std::isfinite(particle.plasticStrain) ||
            particle.plasticStrain < 0.0f) {
            particle.plasticStrain = 0.0f;
        } else {
            particle.plasticStrain =
                std::min(particle.plasticStrain, 100.0f);
        }
        if (particle.mass <= 0.0f || particle.volume0 <= 0.0f) continue;
        const double density =
            static_cast<double>(particle.mass) / particle.volume0;
        const MpmMat2 elasticF = particle.F * particle.Fp.inverse();
        const MpmPositiveSingularValues2D elasticSingularValues =
            positiveMpmSingularValues2D(elasticF);
        double acousticSpeedSquaredUpperBound =
            static_cast<double>(lambda_) + 2.0 * mu_;
        if (elasticSingularValues.valid &&
            elasticSingularValues.singular0 > 1.0e-8f &&
            elasticSingularValues.singular1 > 1.0e-8f) {
            const double stateUpperBound =
                granularHenckyAcousticSpeedSquaredUpperBound(
                    elasticSingularValues.singular0,
                    elasticSingularValues.singular1, lambda_, mu_);
            if (std::isfinite(stateUpperBound) && stateUpperBound > 0.0) {
                acousticSpeedSquaredUpperBound =
                    std::max(acousticSpeedSquaredUpperBound, stateUpperBound);
            }
        }
        const double particleWaveSpeedSquared =
            acousticSpeedSquaredUpperBound / density;
        if (!std::isfinite(particleWaveSpeedSquared) ||
            particleWaveSpeedSquared <= 0.0) {
            return;
        }
        maximumWaveSpeedSquared =
            std::max(maximumWaveSpeedSquared, particleWaveSpeedSquared);
        const double particleSpeed = std::hypot(
            static_cast<double>(particle.vel.x),
            static_cast<double>(particle.vel.y));
        const double affineGradientMagnitude = std::hypot(
            std::hypot(static_cast<double>(particle.C.m00),
                       static_cast<double>(particle.C.m01)),
            std::hypot(static_cast<double>(particle.C.m10),
                       static_cast<double>(particle.C.m11)));
        const double affineVelocityBound =
            kMaximumStencilRadiusInCells * static_cast<double>(cellSize_) *
            affineGradientMagnitude;
        maximumParticleAdvectionSpeed = std::max(
            maximumParticleAdvectionSpeed, particleSpeed + affineVelocityBound);
    }
    if (!std::isfinite(maximumWaveSpeedSquared) ||
        maximumWaveSpeedSquared <= 0.0 ||
        !std::isfinite(maximumParticleAdvectionSpeed)) {
        return;
    }
    const double waveSpeed = std::sqrt(maximumWaveSpeedSquared);
    const double maximumCflSpeed =
        kCfl * static_cast<double>(cellSize_) / kMinimumTimeStep;
    const double maximumAdvectionSpeed = maximumCflSpeed - waveSpeed;
    if (!std::isfinite(maximumAdvectionSpeed) ||
        maximumAdvectionSpeed <= 0.0) {
        // The wave speed alone exceeds the smallest timestep's CFL budget.
        // No admissible timestep can satisfy the bound, but retaining a
        // previous, larger timestep would make the overshoot worse.
        setTimeStep(static_cast<float>(kMinimumTimeStep));
        return;
    }
    if (maximumParticleAdvectionSpeed > maximumAdvectionSpeed) {
        constexpr double kCflSafetyMargin = 0.999;
        const double boundedAdvectionSpeed =
            maximumAdvectionSpeed * kCflSafetyMargin;
        for (auto& particle : particles_) {
            if (!particle.active || particle.mass <= 0.0f ||
                particle.volume0 <= 0.0f) {
                continue;
            }
            const double particleSpeed = std::hypot(
                static_cast<double>(particle.vel.x),
                static_cast<double>(particle.vel.y));
            const double affineGradientMagnitude = std::hypot(
                std::hypot(static_cast<double>(particle.C.m00),
                           static_cast<double>(particle.C.m01)),
                std::hypot(static_cast<double>(particle.C.m10),
                           static_cast<double>(particle.C.m11)));
            const double affineVelocityBound =
                kMaximumStencilRadiusInCells *
                static_cast<double>(cellSize_) * affineGradientMagnitude;
            const double particleAdvectionBound =
                particleSpeed + affineVelocityBound;
            if (!std::isfinite(particleAdvectionBound) ||
                particleAdvectionBound <= boundedAdvectionSpeed) {
                continue;
            }
            const float scale = static_cast<float>(
                boundedAdvectionSpeed / particleAdvectionBound);
            particle.vel.x *= scale;
            particle.vel.y *= scale;
            particle.C.m00 *= scale;
            particle.C.m01 *= scale;
            particle.C.m10 *= scale;
            particle.C.m11 *= scale;
        }
        maximumParticleAdvectionSpeed = boundedAdvectionSpeed;
    }
    const double characteristicSpeed = waveSpeed + maximumParticleAdvectionSpeed;
    if (!std::isfinite(characteristicSpeed) || characteristicSpeed <= 0.0) {
        return;
    }
    const double candidateTimeStep = std::min(
        kMaximumTimeStep, kCfl * static_cast<double>(cellSize_) /
                              characteristicSpeed);
    if (std::isfinite(candidateTimeStep) && candidateTimeStep > 0.0) {
        float roundedTimeStep = static_cast<float>(candidateTimeStep);
        if (static_cast<double>(roundedTimeStep) > candidateTimeStep) {
            roundedTimeStep = std::nextafter(roundedTimeStep, 0.0f);
        }
        setTimeStep(roundedTimeStep);
    }
}

void MpmSolver2D::stepOnce(float h) {

    resetGrid();
    particleToGrid();
    if (constitutiveModel_ != MpmConstitutiveModel::GranularDruckerPrager) {
        applyBoundaryConditions();
    }
    computeGridForces();
    updateGridVelocities(h);
    applyBoundaryConditions();
    gridToParticle(h);
    if (constitutiveModel_ != MpmConstitutiveModel::GranularDruckerPrager) {
        updateDeformationGradient(h);
    }
    applyPlasticity();
    resolveColliders();
    checkFracture();

    // particle-level boundary
    if (hasBoundary_ ||
        (constitutiveModel_ ==
             MpmConstitutiveModel::GranularDruckerPrager &&
         hasGranularFloor_)) {
        const bool granularSand = constitutiveModel_ ==
            MpmConstitutiveModel::GranularDruckerPrager;
        const float floorY = hasBoundary_ ? bymax_ : granularFloorY_;
        const float cellMargin = granularSand
            ? std::min(cellSize_ * 0.5f, granularSupportRadiusLimit_)
            : cellSize_ * 0.5f;
        constexpr float kTwoOverPi = 0.63661977236758134308f;
        for (auto& p : particles_) {
            if (!p.active) continue;
            // Repair penetration, but remove normal motion only when it still
            // points out through the contacted wall or granular floor.
            float margin = cellMargin;
            if (granularSand && std::isfinite(p.volume0) &&
                p.volume0 > 0.0f) {
                const float deformationDeterminant = p.F.det();
                if (std::isfinite(deformationDeterminant) &&
                    deformationDeterminant > 0.0f) {
                    const float currentVolume =
                        p.volume0 * deformationDeterminant;
                    if (std::isfinite(currentVolume) && currentVolume > 0.0f) {
                        const float supportRadius =
                            1.25f * std::sqrt(currentVolume * kTwoOverPi);
                        if (std::isfinite(supportRadius) &&
                            supportRadius > 0.0f) {
                            margin = std::min({cellMargin, supportRadius,
                                               granularSupportRadiusLimit_});
                        }
                    }
                }
            }
            if (hasBoundary_ && p.pos.x < bxmin_ + margin) {
                p.pos.x = bxmin_ + margin;
                const bool movingOutward = p.vel.x < 0.0f;
                if (!granularSand || movingOutward) p.vel.x = 0.0f;
                if (granularSand && movingOutward) {
                    projectGranularAffineVelocityToTangent(p, -1.0f, 0.0f);
                }
            }
            if (hasBoundary_ && p.pos.x > bxmax_ - margin) {
                p.pos.x = bxmax_ - margin;
                const bool movingOutward = p.vel.x > 0.0f;
                if (!granularSand || movingOutward) p.vel.x = 0.0f;
                if (granularSand && movingOutward) {
                    projectGranularAffineVelocityToTangent(p, 1.0f, 0.0f);
                }
            }
            if (hasBoundary_ && p.pos.y < bymin_ + margin) {
                p.pos.y = bymin_ + margin;
                const bool movingOutward = p.vel.y < 0.0f;
                if (!granularSand || movingOutward) p.vel.y = 0.0f;
                if (granularSand && movingOutward) {
                    projectGranularAffineVelocityToTangent(p, 0.0f, -1.0f);
                }
            }
            if ((hasBoundary_ || (granularSand && hasGranularFloor_)) &&
                p.pos.y > floorY - margin) {
                p.pos.y = floorY - margin;
                const bool movingOutward = p.vel.y > 0.0f;
                if (!granularSand || movingOutward) p.vel.y = 0.0f;
                if (granularSand && movingOutward) {
                    projectGranularAffineVelocityToTangent(p, 0.0f, 1.0f);
                }
            }
        }
    }

    // NaN guard: zero non-finite states to prevent propagation
    float boundsMinimumX = std::numeric_limits<float>::max();
    float boundsMinimumY = std::numeric_limits<float>::max();
    float boundsMaximumX = std::numeric_limits<float>::lowest();
    float boundsMaximumY = std::numeric_limits<float>::lowest();
    bool hasFiniteParticleBounds = false;
    for (auto& p : particles_) {
        if (!std::isfinite(p.pos.x) || !std::isfinite(p.pos.y) ||
            !std::isfinite(p.vel.x) || !std::isfinite(p.vel.y)) {
            p.pos.x = std::isfinite(p.pos.x) ? p.pos.x : 0.0f;
            p.pos.y = std::isfinite(p.pos.y) ? p.pos.y : 0.0f;
            p.vel.x = 0.0f; p.vel.y = 0.0f;
        }
        if (p.active && std::isfinite(p.pos.x) && std::isfinite(p.pos.y)) {
            boundsMinimumX = std::min(boundsMinimumX, p.pos.x);
            boundsMinimumY = std::min(boundsMinimumY, p.pos.y);
            boundsMaximumX = std::max(boundsMaximumX, p.pos.x);
            boundsMaximumY = std::max(boundsMaximumY, p.pos.y);
            hasFiniteParticleBounds = true;
        }
        if (!std::isfinite(p.F.m00) || !std::isfinite(p.F.m01) ||
            !std::isfinite(p.F.m10) || !std::isfinite(p.F.m11)) {
            p.F = MpmMat2::identity();
        }
        if (constitutiveModel_ ==
            MpmConstitutiveModel::GranularDruckerPrager) {
            const auto finiteMatrix = [](const MpmMat2& matrix) noexcept {
                return std::isfinite(matrix.m00) &&
                       std::isfinite(matrix.m01) &&
                       std::isfinite(matrix.m10) &&
                       std::isfinite(matrix.m11);
            };
            const float deformationDeterminant = p.F.det();
            if (!finiteMatrix(p.F) ||
                !std::isfinite(deformationDeterminant) ||
                deformationDeterminant <= 1.0e-12f) {
                p.F = MpmMat2::identity();
                p.C = MpmMat2::zero();
                p.vel = MpmVec2{};
            }
            const float plasticDeterminant = p.Fp.det();
            if (!finiteMatrix(p.Fp) || !std::isfinite(plasticDeterminant) ||
                plasticDeterminant <= 1.0e-12f) {
                p.Fp = MpmMat2::identity();
                p.C = MpmMat2::zero();
                p.vel = MpmVec2{};
            } else if (!finiteMatrix(p.C)) {
                p.C = MpmMat2::zero();
                p.vel = MpmVec2{};
            }
            if (!std::isfinite(p.plasticStrain) || p.plasticStrain < 0.0f) {
                p.plasticStrain = 0.0f;
            }
        }
    }
    particleBoundsCache_ = hasFiniteParticleBounds
        ? MpmBounds2D{boundsMinimumX, boundsMinimumY,
                      boundsMaximumX, boundsMaximumY, true}
        : MpmBounds2D{};
    particleBoundsCacheValid_ = true;
}

} // namespace ArtifactCore
