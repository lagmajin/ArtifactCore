module;
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>
#include <string>

module Core.Simulation.Pyro;

namespace ArtifactCore {

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline void hashAppend(std::uint64_t& seed, std::uint64_t value) noexcept {
    seed ^= value;
    seed *= kFnvPrime;
}

inline void hashAppendFloat(std::uint64_t& seed, float value) noexcept {
    hashAppend(seed, std::bit_cast<std::uint32_t>(value));
}

inline std::size_t indexOf(const PyroResolution& resolution, int x, int y, int z) noexcept {
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(resolution.width) * (
            static_cast<std::size_t>(y)
            + static_cast<std::size_t>(resolution.height) * static_cast<std::size_t>(z));
}

inline PyroVec3 clampPositionToDomain(const PyroDomain& domain, const PyroVec3& position) noexcept {
    return {
        std::clamp(position.x, domain.min.x, domain.max.x),
        std::clamp(position.y, domain.min.y, domain.max.y),
        std::clamp(position.z, domain.min.z, domain.max.z)
    };
}

inline float cellToWorldStep(const PyroResolution& resolution, float domainExtent, int cells) noexcept {
    if (cells <= 0) {
        return 0.0f;
    }
    return domainExtent / static_cast<float>(cells);
}

}

PyroResolution PyroDomain::resolution() const noexcept {
    if (voxelSize <= 0.0f) {
        return {};
    }
    const auto computeAxis = [this](float minValue, float maxValue) -> int {
        const float extent = std::max(0.0f, maxValue - minValue);
        return std::max(1, static_cast<int>(std::ceil(extent / voxelSize)));
    };
    return {computeAxis(min.x, max.x), computeAxis(min.y, max.y), computeAxis(min.z, max.z)};
}

float PyroDomain::width() const noexcept { return std::max(0.0f, max.x - min.x); }
float PyroDomain::height() const noexcept { return std::max(0.0f, max.y - min.y); }
float PyroDomain::depth() const noexcept { return std::max(0.0f, max.z - min.z); }

bool PyroScalarFieldView::empty() const noexcept { return values.empty(); }
std::size_t PyroScalarFieldView::size() const noexcept { return values.size(); }
float& PyroScalarFieldView::at(int x, int y, int z) const { return values[indexOf(resolution, x, y, z)]; }

bool PyroConstScalarFieldView::empty() const noexcept { return values.empty(); }
std::size_t PyroConstScalarFieldView::size() const noexcept { return values.size(); }
const float& PyroConstScalarFieldView::at(int x, int y, int z) const { return values[indexOf(resolution, x, y, z)]; }

bool PyroVectorFieldView::empty() const noexcept { return values.empty(); }
std::size_t PyroVectorFieldView::size() const noexcept { return values.size(); }
PyroVec3& PyroVectorFieldView::at(int x, int y, int z) const { return values[indexOf(resolution, x, y, z)]; }

bool PyroConstVectorFieldView::empty() const noexcept { return values.empty(); }
std::size_t PyroConstVectorFieldView::size() const noexcept { return values.size(); }
const PyroVec3& PyroConstVectorFieldView::at(int x, int y, int z) const { return values[indexOf(resolution, x, y, z)]; }

void PyroFieldSet::resize(PyroResolution resolution) {
    resolution_ = resolution;
    const auto count = cellCount();
    density_.assign(count, 0.0f);
    temperature_.assign(count, 0.0f);
    fuel_.assign(count, 0.0f);
    pressure_.assign(count, 0.0f);
    divergence_.assign(count, 0.0f);
    velocity_.assign(count, {});
}

void PyroFieldSet::clear() {
    std::fill(density_.begin(), density_.end(), 0.0f);
    std::fill(temperature_.begin(), temperature_.end(), 0.0f);
    std::fill(fuel_.begin(), fuel_.end(), 0.0f);
    std::fill(pressure_.begin(), pressure_.end(), 0.0f);
    std::fill(divergence_.begin(), divergence_.end(), 0.0f);
    std::fill(velocity_.begin(), velocity_.end(), PyroVec3{});
}

std::size_t PyroFieldSet::cellCount() const noexcept {
    if (resolution_.width <= 0 || resolution_.height <= 0 || resolution_.depth <= 0) {
        return 0;
    }
    return static_cast<std::size_t>(resolution_.width)
        * static_cast<std::size_t>(resolution_.height)
        * static_cast<std::size_t>(resolution_.depth);
}

PyroScalarFieldView PyroFieldSet::densityView() noexcept { return {std::span<float>(density_), resolution_}; }
PyroConstScalarFieldView PyroFieldSet::densityView() const noexcept { return {std::span<const float>(density_), resolution_}; }
PyroScalarFieldView PyroFieldSet::temperatureView() noexcept { return {std::span<float>(temperature_), resolution_}; }
PyroConstScalarFieldView PyroFieldSet::temperatureView() const noexcept { return {std::span<const float>(temperature_), resolution_}; }
PyroScalarFieldView PyroFieldSet::fuelView() noexcept { return {std::span<float>(fuel_), resolution_}; }
PyroConstScalarFieldView PyroFieldSet::fuelView() const noexcept { return {std::span<const float>(fuel_), resolution_}; }
PyroScalarFieldView PyroFieldSet::pressureView() noexcept { return {std::span<float>(pressure_), resolution_}; }
PyroConstScalarFieldView PyroFieldSet::pressureView() const noexcept { return {std::span<const float>(pressure_), resolution_}; }
PyroScalarFieldView PyroFieldSet::divergenceView() noexcept { return {std::span<float>(divergence_), resolution_}; }
PyroConstScalarFieldView PyroFieldSet::divergenceView() const noexcept { return {std::span<const float>(divergence_), resolution_}; }
PyroVectorFieldView PyroFieldSet::velocityView() noexcept { return {std::span<PyroVec3>(velocity_), resolution_}; }
PyroConstVectorFieldView PyroFieldSet::velocityView() const noexcept { return {std::span<const PyroVec3>(velocity_), resolution_}; }

PyroMemoryEstimate PyroFieldSet::estimateMemory() const noexcept {
    const auto count = cellCount();
    PyroMemoryEstimate estimate{};
    estimate.resolution = resolution_;
    estimate.densityBytes = count * sizeof(float);
    estimate.temperatureBytes = count * sizeof(float);
    estimate.fuelBytes = count * sizeof(float);
    estimate.pressureBytes = count * sizeof(float);
    estimate.divergenceBytes = count * sizeof(float);
    estimate.velocityBytes = count * sizeof(PyroVec3);
    estimate.totalBytes = estimate.densityBytes + estimate.temperatureBytes + estimate.fuelBytes +
        estimate.pressureBytes + estimate.divergenceBytes + estimate.velocityBytes;
    return estimate;
}

float PyroFieldSet::sampleScalar(const PyroConstScalarFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept {
    if (view.empty()) {
        return 0.0f;
    }
    if (boundaryMode == PyroBoundaryMode::Open
        && (position.x < 0.0f || position.y < 0.0f || position.z < 0.0f
            || position.x > static_cast<float>(view.resolution.width - 1)
            || position.y > static_cast<float>(view.resolution.height - 1)
            || position.z > static_cast<float>(view.resolution.depth - 1))) {
        return 0.0f;
    }
    const float fx = std::clamp(position.x, 0.0f, static_cast<float>(view.resolution.width - 1));
    const float fy = std::clamp(position.y, 0.0f, static_cast<float>(view.resolution.height - 1));
    const float fz = std::clamp(position.z, 0.0f, static_cast<float>(view.resolution.depth - 1));
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int z0 = static_cast<int>(std::floor(fz));
    const int x1 = std::min(x0 + 1, view.resolution.width - 1);
    const int y1 = std::min(y0 + 1, view.resolution.height - 1);
    const int z1 = std::min(z0 + 1, view.resolution.depth - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float tz = fz - static_cast<float>(z0);
    const auto sample = [&](int x, int y, int z) { return view.values[indexOf(view.resolution, x, y, z)]; };
    const float c000 = sample(x0, y0, z0);
    const float c100 = sample(x1, y0, z0);
    const float c010 = sample(x0, y1, z0);
    const float c110 = sample(x1, y1, z0);
    const float c001 = sample(x0, y0, z1);
    const float c101 = sample(x1, y0, z1);
    const float c011 = sample(x0, y1, z1);
    const float c111 = sample(x1, y1, z1);
    const float c00 = c000 + (c100 - c000) * tx;
    const float c10 = c010 + (c110 - c010) * tx;
    const float c01 = c001 + (c101 - c001) * tx;
    const float c11 = c011 + (c111 - c011) * tx;
    const float c0 = c00 + (c10 - c00) * ty;
    const float c1 = c01 + (c11 - c01) * ty;
    return c0 + (c1 - c0) * tz;
}

PyroVec3 PyroFieldSet::sampleVector(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept {
    if (view.empty() || view.resolution.width <= 0 || view.resolution.height <= 0 || view.resolution.depth <= 0) {
        return {};
    }
    if (boundaryMode == PyroBoundaryMode::Open
        && (position.x < 0.0f || position.y < 0.0f || position.z < 0.0f
            || position.x > static_cast<float>(view.resolution.width - 1)
            || position.y > static_cast<float>(view.resolution.height - 1)
            || position.z > static_cast<float>(view.resolution.depth - 1))) {
        return {};
    }
    const float fx = std::clamp(position.x, 0.0f, static_cast<float>(view.resolution.width - 1));
    const float fy = std::clamp(position.y, 0.0f, static_cast<float>(view.resolution.height - 1));
    const float fz = std::clamp(position.z, 0.0f, static_cast<float>(view.resolution.depth - 1));
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int z0 = static_cast<int>(std::floor(fz));
    const int x1 = std::min(x0 + 1, view.resolution.width - 1);
    const int y1 = std::min(y0 + 1, view.resolution.height - 1);
    const int z1 = std::min(z0 + 1, view.resolution.depth - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float tz = fz - static_cast<float>(z0);
    const auto sample = [&](int x, int y, int z) { return view.values[indexOf(view.resolution, x, y, z)]; };
    const auto lerpVec = [](const PyroVec3& a, const PyroVec3& b, float t) {
        return PyroVec3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    };
    const PyroVec3 c000 = sample(x0, y0, z0);
    const PyroVec3 c100 = sample(x1, y0, z0);
    const PyroVec3 c010 = sample(x0, y1, z0);
    const PyroVec3 c110 = sample(x1, y1, z0);
    const PyroVec3 c001 = sample(x0, y0, z1);
    const PyroVec3 c101 = sample(x1, y0, z1);
    const PyroVec3 c011 = sample(x0, y1, z1);
    const PyroVec3 c111 = sample(x1, y1, z1);
    const PyroVec3 c00 = lerpVec(c000, c100, tx);
    const PyroVec3 c10 = lerpVec(c010, c110, tx);
    const PyroVec3 c01 = lerpVec(c001, c101, tx);
    const PyroVec3 c11 = lerpVec(c011, c111, tx);
    const PyroVec3 c0 = lerpVec(c00, c10, ty);
    const PyroVec3 c1 = lerpVec(c01, c11, ty);
    return lerpVec(c0, c1, tz);
}

PyroVec3 PyroFieldSet::gradient(const PyroConstScalarFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept {
    const float h = 1.0f;
    const PyroVec3 px{position.x + h, position.y, position.z};
    const PyroVec3 mx{position.x - h, position.y, position.z};
    const PyroVec3 py{position.x, position.y + h, position.z};
    const PyroVec3 my{position.x, position.y - h, position.z};
    const PyroVec3 pz{position.x, position.y, position.z + h};
    const PyroVec3 mz{position.x, position.y, position.z - h};
    return {
        (sampleScalar(view, px, boundaryMode) - sampleScalar(view, mx, boundaryMode)) * 0.5f,
        (sampleScalar(view, py, boundaryMode) - sampleScalar(view, my, boundaryMode)) * 0.5f,
        (sampleScalar(view, pz, boundaryMode) - sampleScalar(view, mz, boundaryMode)) * 0.5f
    };
}

float PyroFieldSet::divergence(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept {
    const float dx = sampleVector(view, {position.x + 1.0f, position.y, position.z}, boundaryMode).x
        - sampleVector(view, {position.x - 1.0f, position.y, position.z}, boundaryMode).x;
    const float dy = sampleVector(view, {position.x, position.y + 1.0f, position.z}, boundaryMode).y
        - sampleVector(view, {position.x, position.y - 1.0f, position.z}, boundaryMode).y;
    const float dz = sampleVector(view, {position.x, position.y, position.z + 1.0f}, boundaryMode).z
        - sampleVector(view, {position.x, position.y, position.z - 1.0f}, boundaryMode).z;
    return (dx + dy + dz) * 0.5f;
}

float PyroFieldSet::curlMagnitude(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept {
    const PyroVec3 right = sampleVector(view, {position.x + 1.0f, position.y, position.z}, boundaryMode);
    const PyroVec3 left = sampleVector(view, {position.x - 1.0f, position.y, position.z}, boundaryMode);
    const PyroVec3 up = sampleVector(view, {position.x, position.y + 1.0f, position.z}, boundaryMode);
    const PyroVec3 down = sampleVector(view, {position.x, position.y - 1.0f, position.z}, boundaryMode);
    const PyroVec3 front = sampleVector(view, {position.x, position.y, position.z + 1.0f}, boundaryMode);
    const PyroVec3 back = sampleVector(view, {position.x, position.y, position.z - 1.0f}, boundaryMode);
    const PyroVec3 curl{
        (up.z - down.z) * 0.5f - (front.y - back.y) * 0.5f,
        (front.x - back.x) * 0.5f - (right.z - left.z) * 0.5f,
        (right.y - left.y) * 0.5f - (up.x - down.x) * 0.5f
    };
    return std::sqrt(curl.x * curl.x + curl.y * curl.y + curl.z * curl.z);
}

PyroSimulation::PyroSimulation(PyroDomain domain) { setDomain(domain); }

void PyroSimulation::setDomain(PyroDomain domain) {
    domain_ = domain;
    fields_.resize(domain_.resolution());
}

void PyroSimulation::setSettings(PyroSimulationSettings settings) { settings_ = settings; }

void PyroSimulation::setFixedTimeStep(double fixedTimeStep) noexcept {
    fixedTimeStep_ = fixedTimeStep > 0.0 ? fixedTimeStep : 1.0 / 60.0;
}

void PyroSimulation::setCacheInterval(std::uint64_t cacheInterval) noexcept {
    cacheInterval_ = cacheInterval == 0 ? 1 : cacheInterval;
}

void PyroSimulation::setCacheDirectory(std::filesystem::path cacheDirectory) {
    cacheDirectory_ = std::move(cacheDirectory);
}

void PyroSimulation::reset() {
    fields_.clear();
    frameIndex_ = 0;
    timeSeconds_ = 0.0;
    accumulator_ = 0.0;
    checkpointCache_.clear();
}

void PyroSimulation::seek(std::uint64_t frameIndex, double timeSeconds) {
    if (restoreCheckpoint(frameIndex)) {
        timeSeconds_ = timeSeconds;
        return;
    }
    frameIndex_ = frameIndex;
    timeSeconds_ = timeSeconds;
}

bool PyroSimulation::isInsideDomain(const PyroVec3& position) const noexcept {
    return position.x >= domain_.min.x && position.x <= domain_.max.x
        && position.y >= domain_.min.y && position.y <= domain_.max.y
        && position.z >= domain_.min.z && position.z <= domain_.max.z;
}

std::size_t PyroSimulation::cellIndex(int x, int y, int z) const noexcept {
    return indexOf(fields_.resolution(), x, y, z);
}

void PyroSimulation::setSources(std::span<const PyroSourceState> sources) {
    ownedSources_.assign(sources.begin(), sources.end());
    sources_ = ownedSources_;
}

void PyroSimulation::setColliders(std::span<const PyroColliderState> colliders) {
    ownedColliders_.assign(colliders.begin(), colliders.end());
    colliders_ = ownedColliders_;
}

void PyroSimulation::storeCheckpoint() {
    PyroFieldSnapshot snapshot{};
    snapshot.resolution = fields_.resolution();
    snapshot.density = fields_.densityStorage();
    snapshot.temperature = fields_.temperatureStorage();
    snapshot.fuel = fields_.fuelStorage();
    snapshot.pressure = fields_.pressureStorage();
    snapshot.divergence = fields_.divergenceStorage();
    snapshot.velocity = fields_.velocityStorage();
    checkpointCache_[frameIndex_] = std::move(snapshot);
    if (!cacheDirectory_.empty()) {
        const auto path = cacheDirectory_ / (std::to_string(frameIndex_) + ".pyroc");
        saveCheckpointSnapshot(checkpointCache_[frameIndex_], path);
    }
}

bool PyroSimulation::saveCheckpointSnapshot(const PyroFieldSnapshot& snapshot, const std::filesystem::path& path) const {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const std::uint32_t magic = 0x5059524Fu; // PYRO
    const std::uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&snapshot.resolution), sizeof(snapshot.resolution));
    const auto writeVector = [&out](const auto& data) {
        const std::uint64_t size = static_cast<std::uint64_t>(data.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (size > 0) {
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(data[0])));
        }
    };

    writeVector(snapshot.density);
    writeVector(snapshot.temperature);
    writeVector(snapshot.fuel);
    writeVector(snapshot.pressure);
    writeVector(snapshot.divergence);
    writeVector(snapshot.velocity);
    return static_cast<bool>(out);
}

bool PyroSimulation::loadCheckpointSnapshot(PyroFieldSnapshot& snapshot, const std::filesystem::path& path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in || magic != 0x5059524Fu || version != 1) {
        return false;
    }

    in.read(reinterpret_cast<char*>(&snapshot.resolution), sizeof(snapshot.resolution));
    if (!in) {
        return false;
    }

    const auto readVector = [&in](auto& data) {
        std::uint64_t size = 0;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!in) {
            return false;
        }
        data.resize(static_cast<std::size_t>(size));
        if (size > 0) {
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(data[0])));
        }
        return static_cast<bool>(in);
    };

    return readVector(snapshot.density)
        && readVector(snapshot.temperature)
        && readVector(snapshot.fuel)
        && readVector(snapshot.pressure)
        && readVector(snapshot.divergence)
        && readVector(snapshot.velocity);
}

PyroCacheStatus PyroSimulation::saveCheckpointToDisk(std::uint64_t frameIndex) const {
    PyroCacheStatus status{};
    status.supported = !cacheDirectory_.empty();
    if (!status.supported) {
        status.reason = "cache directory is empty";
        return status;
    }

    const auto it = checkpointCache_.find(frameIndex);
    if (it == checkpointCache_.end()) {
        status.reason = "checkpoint not found in memory";
        return status;
    }

    const auto path = cacheDirectory_ / (std::to_string(frameIndex) + ".pyroc");
    status.loaded = saveCheckpointSnapshot(it->second, path);
    if (!status.loaded) {
        status.reason = "failed to write checkpoint";
    }
    return status;
}

PyroCacheStatus PyroSimulation::loadCheckpointFromDisk(std::uint64_t frameIndex) {
    PyroCacheStatus status{};
    status.supported = !cacheDirectory_.empty();
    if (!status.supported) {
        status.reason = "cache directory is empty";
        return status;
    }

    PyroFieldSnapshot snapshot{};
    const auto path = cacheDirectory_ / (std::to_string(frameIndex) + ".pyroc");
    status.loaded = loadCheckpointSnapshot(snapshot, path);
    if (!status.loaded) {
        status.reason = "failed to read checkpoint";
        return status;
    }

    fields_.resize(snapshot.resolution);
    fields_.densityStorage() = std::move(snapshot.density);
    fields_.temperatureStorage() = std::move(snapshot.temperature);
    fields_.fuelStorage() = std::move(snapshot.fuel);
    fields_.pressureStorage() = std::move(snapshot.pressure);
    fields_.divergenceStorage() = std::move(snapshot.divergence);
    fields_.velocityStorage() = std::move(snapshot.velocity);
    frameIndex_ = frameIndex;
    checkpointCache_[frameIndex] = PyroFieldSnapshot{
        .resolution = fields_.resolution(),
        .density = fields_.densityStorage(),
        .temperature = fields_.temperatureStorage(),
        .fuel = fields_.fuelStorage(),
        .pressure = fields_.pressureStorage(),
        .divergence = fields_.divergenceStorage(),
        .velocity = fields_.velocityStorage()
    };
    return status;
}

bool PyroSimulation::restoreCheckpoint(std::uint64_t frameIndex) {
    const auto it = checkpointCache_.find(frameIndex);
    if (it == checkpointCache_.end()) {
        if (cacheDirectory_.empty()) {
            return false;
        }
        auto status = loadCheckpointFromDisk(frameIndex);
        return status.loaded;
    }
    const auto& snapshot = it->second;
    fields_.resize(snapshot.resolution);
    fields_.densityStorage() = snapshot.density;
    fields_.temperatureStorage() = snapshot.temperature;
    fields_.fuelStorage() = snapshot.fuel;
    fields_.pressureStorage() = snapshot.pressure;
    fields_.divergenceStorage() = snapshot.divergence;
    fields_.velocityStorage() = snapshot.velocity;
    frameIndex_ = frameIndex;
    return true;
}

bool PyroSimulation::isInsideCollider(const PyroColliderState& collider, const PyroVec3& position) const noexcept {
    if (!collider.enabled) {
        return false;
    }
    const PyroVec3 delta{
        position.x - collider.center.x,
        position.y - collider.center.y,
        position.z - collider.center.z
    };
    if (collider.type == PyroColliderType::Sphere) {
        const float radius = std::max({collider.extent.x, collider.extent.y, collider.extent.z, 0.0f});
        return (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) <= radius * radius;
    }
    return std::abs(delta.x) <= collider.extent.x
        && std::abs(delta.y) <= collider.extent.y
        && std::abs(delta.z) <= collider.extent.z;
}

void PyroSimulation::applyColliders() {
    const auto resolution = fields_.resolution();
    auto velocity = fields_.velocityView();
    auto density = fields_.densityView();
    auto temperature = fields_.temperatureView();
    auto fuel = fields_.fuelView();

    for (const auto& collider : colliders_) {
        if (!collider.enabled) {
            continue;
        }

        const int cx = static_cast<int>((collider.center.x - domain_.min.x) / domain_.voxelSize);
        const int cy = static_cast<int>((collider.center.y - domain_.min.y) / domain_.voxelSize);
        const int cz = static_cast<int>((collider.center.z - domain_.min.z) / domain_.voxelSize);
        const int rx = std::max(0, static_cast<int>(std::ceil(collider.extent.x / std::max(domain_.voxelSize, 1e-6f))));
        const int ry = std::max(0, static_cast<int>(std::ceil(collider.extent.y / std::max(domain_.voxelSize, 1e-6f))));
        const int rz = std::max(0, static_cast<int>(std::ceil(collider.extent.z / std::max(domain_.voxelSize, 1e-6f))));

        for (int z = std::max(0, cz - rz); z <= std::min(resolution.depth - 1, cz + rz); ++z) {
            for (int y = std::max(0, cy - ry); y <= std::min(resolution.height - 1, cy + ry); ++y) {
                for (int x = std::max(0, cx - rx); x <= std::min(resolution.width - 1, cx + rx); ++x) {
                    const PyroVec3 samplePos{
                        (static_cast<float>(x) + 0.5f) * domain_.voxelSize + domain_.min.x,
                        (static_cast<float>(y) + 0.5f) * domain_.voxelSize + domain_.min.y,
                        (static_cast<float>(z) + 0.5f) * domain_.voxelSize + domain_.min.z
                    };
                    if (!isInsideCollider(collider, samplePos)) {
                        continue;
                    }
                    const auto idx = cellIndex(x, y, z);
                    density.values[idx] = 0.0f;
                    temperature.values[idx] = 0.0f;
                    fuel.values[idx] = 0.0f;
                    if (collider.noSlip) {
                        velocity.values[idx] = collider.velocity;
                    }
                }
            }
        }
    }
}

void PyroSimulation::applyCombustion(float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    const auto resolution = fields_.resolution();
    auto density = fields_.densityView();
    auto temperature = fields_.temperatureView();
    auto fuel = fields_.fuelView();
    auto velocity = fields_.velocityView();

    const float dt = deltaSeconds;
    const float ignitionThreshold = 0.35f;
    const float burnRate = 0.65f;
    const float heatRelease = 1.4f;
    const float smokeYield = 0.9f;

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                const float ignite = std::clamp(temperature.values[idx] + fuel.values[idx] - ignitionThreshold, 0.0f, 1.0f);
                const float burned = std::min(fuel.values[idx], burnRate * ignite * dt);
                if (burned <= 0.0f) {
                    continue;
                }
                fuel.values[idx] -= burned;
                temperature.values[idx] += burned * heatRelease;
                density.values[idx] += burned * smokeYield;
                velocity.values[idx].y += burned * 0.15f;
            }
        }
    }
}

void PyroSimulation::applyVorticityConfinement(float deltaSeconds) {
    if (settings_.vorticity <= 0.0f || deltaSeconds <= 0.0f) {
        return;
    }

    const auto resolution = fields_.resolution();
    auto velocity = fields_.velocityView();
    const auto curl = fields_.velocityView();
    const float dt = static_cast<float>(deltaSeconds);

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                const int xm = std::max(0, x - 1);
                const int xp = std::min(resolution.width - 1, x + 1);
                const int ym = std::max(0, y - 1);
                const int yp = std::min(resolution.height - 1, y + 1);
                const int zm = std::max(0, z - 1);
                const int zp = std::min(resolution.depth - 1, z + 1);
                const float wx = (curl.values[cellIndex(xp, y, z)].y - curl.values[cellIndex(xm, y, z)].y) * 0.5f;
                const float wy = (curl.values[cellIndex(x, yp, z)].z - curl.values[cellIndex(x, ym, z)].z) * 0.5f;
                const float wz = (curl.values[cellIndex(x, y, zp)].x - curl.values[cellIndex(x, y, zm)].x) * 0.5f;
                const float len = std::sqrt(wx * wx + wy * wy + wz * wz) + 1e-5f;
                const float nx = wx / len;
                const float ny = wy / len;
                const float nz = wz / len;
                const float strength = settings_.vorticity * dt;
                velocity.values[idx].x += (ny - nz) * strength;
                velocity.values[idx].y += (nz - nx) * strength;
                velocity.values[idx].z += (nx - ny) * strength;
            }
        }
    }
}

void PyroSimulation::integrateStep(float deltaSeconds) {
    if (paused_ || deltaSeconds <= 0.0) {
        return;
    }
    if (fields_.empty()) {
        fields_.resize(domain_.resolution());
    }

    const auto resolution = fields_.resolution();
    if (resolution.width <= 0 || resolution.height <= 0 || resolution.depth <= 0) {
        return;
    }

    const float dt = static_cast<float>(deltaSeconds);
    auto density = fields_.densityView();
    auto temperature = fields_.temperatureView();
    auto fuel = fields_.fuelView();
    auto pressure = fields_.pressureView();
    auto divergence = fields_.divergenceView();
    auto velocity = fields_.velocityView();

    const auto densityPrev = fields_.densityStorage();
    const auto temperaturePrev = fields_.temperatureStorage();
    const auto fuelPrev = fields_.fuelStorage();
    const auto velocityPrev = fields_.velocityStorage();

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                density.values[idx] = std::max(0.0f, density.values[idx] - settings_.dissipation * dt);
                temperature.values[idx] = std::max(0.0f, temperature.values[idx] - settings_.coolingRate * dt);
                fuel.values[idx] = std::max(0.0f, fuel.values[idx] - settings_.coolingRate * 0.5f * dt);
                pressure.values[idx] = 0.0f;
                divergence.values[idx] = 0.0f;
                velocity.values[idx].x *= std::max(0.0f, 1.0f - settings_.dissipation * dt);
                velocity.values[idx].y *= std::max(0.0f, 1.0f - settings_.dissipation * dt);
                velocity.values[idx].z *= std::max(0.0f, 1.0f - settings_.dissipation * dt);
            }
        }
    }

    for (const auto& source : sources_) {
        if (!source.enabled || !isInsideDomain(source.position)) {
            continue;
        }
        const auto centerX = static_cast<int>((source.position.x - domain_.min.x) / domain_.voxelSize);
        const auto centerY = static_cast<int>((source.position.y - domain_.min.y) / domain_.voxelSize);
        const auto centerZ = static_cast<int>((source.position.z - domain_.min.z) / domain_.voxelSize);
        const auto radiusX = std::max(0, static_cast<int>(std::ceil(source.extent.x / std::max(domain_.voxelSize, 1e-6f))));
        const auto radiusY = std::max(0, static_cast<int>(std::ceil(source.extent.y / std::max(domain_.voxelSize, 1e-6f))));
        const auto radiusZ = std::max(0, static_cast<int>(std::ceil(source.extent.z / std::max(domain_.voxelSize, 1e-6f))));
        const float extentMax = std::max({source.extent.x, source.extent.y, source.extent.z, 1e-6f});

        for (int z = std::max(0, centerZ - radiusZ); z <= std::min(resolution.depth - 1, centerZ + radiusZ); ++z) {
            for (int y = std::max(0, centerY - radiusY); y <= std::min(resolution.height - 1, centerY + radiusY); ++y) {
                for (int x = std::max(0, centerX - radiusX); x <= std::min(resolution.width - 1, centerX + radiusX); ++x) {
                    const auto idx = cellIndex(x, y, z);
                    const float dx = (static_cast<float>(x) + 0.5f) * domain_.voxelSize + domain_.min.x - source.position.x;
                    const float dy = (static_cast<float>(y) + 0.5f) * domain_.voxelSize + domain_.min.y - source.position.y;
                    const float dz = (static_cast<float>(z) + 0.5f) * domain_.voxelSize + domain_.min.z - source.position.z;
                    const float falloff = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) / extentMax, 0.0f, 1.0f);
                    const float strength = falloff * dt;
                    density.values[idx] = std::clamp(density.values[idx] + source.density * settings_.sourceDensity * strength, 0.0f, 1e6f);
                    temperature.values[idx] = std::clamp(temperature.values[idx] + source.temperature * settings_.sourceTemperature * strength, 0.0f, 1e6f);
                    fuel.values[idx] = std::clamp(fuel.values[idx] + source.fuel * settings_.sourceFuel * strength, 0.0f, 1e6f);
                    velocity.values[idx].x += source.velocity.x * strength;
                    velocity.values[idx].y += source.velocity.y * strength;
                    velocity.values[idx].z += source.velocity.z * strength;
                }
            }
        }
    }

    applyCombustion(dt);

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                velocity.values[idx].y += temperature.values[idx] * settings_.buoyancy * dt;
            }
        }
    }

    applyVorticityConfinement(dt);

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                const PyroVec3 pos{
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                };
                const PyroVec3 backtrace{
                    pos.x - velocity.values[idx].x * dt,
                    pos.y - velocity.values[idx].y * dt,
                    pos.z - velocity.values[idx].z * dt
                };
                density.values[idx] = fields_.sampleScalar({std::span<const float>(densityPrev), resolution}, backtrace, domain_.boundaryMode);
                temperature.values[idx] = fields_.sampleScalar({std::span<const float>(temperaturePrev), resolution}, backtrace, domain_.boundaryMode);
                fuel.values[idx] = fields_.sampleScalar({std::span<const float>(fuelPrev), resolution}, backtrace, domain_.boundaryMode);
                velocity.values[idx] = fields_.sampleVector({std::span<const PyroVec3>(velocityPrev), resolution}, backtrace, domain_.boundaryMode);
                divergence.values[idx] = 0.0f;
                pressure.values[idx] = 0.0f;
            }
        }
    }

    computeDivergence();
    solvePressure(std::max(1, static_cast<int>(settings_.pressureIterations)));
    projectVelocity();
    applyColliders();
}

void PyroSimulation::step(double deltaSeconds) {
    if (paused_ || deltaSeconds <= 0.0) {
        return;
    }

    accumulator_ += deltaSeconds;
    const double fixedStep = fixedTimeStep_ > 0.0 ? fixedTimeStep_ : 1.0 / 60.0;
    int substeps = 0;
    while (accumulator_ >= fixedStep && substeps < 8) {
        integrateStep(static_cast<float>(fixedStep));
        accumulator_ -= fixedStep;
        timeSeconds_ += fixedStep;
        ++frameIndex_;
        if (cacheInterval_ > 0 && (frameIndex_ % cacheInterval_ == 0)) {
            storeCheckpoint();
        }
        ++substeps;
    }
}

void PyroSimulation::computeDivergence() {
    const auto resolution = fields_.resolution();
    auto divergence = fields_.divergenceView();
    const auto velocity = fields_.velocityView();

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                const int xm = std::max(0, x - 1);
                const int xp = std::min(resolution.width - 1, x + 1);
                const int ym = std::max(0, y - 1);
                const int yp = std::min(resolution.height - 1, y + 1);
                const int zm = std::max(0, z - 1);
                const int zp = std::min(resolution.depth - 1, z + 1);
                const PyroVec3 vxm = velocity.values[cellIndex(xm, y, z)];
                const PyroVec3 vxp = velocity.values[cellIndex(xp, y, z)];
                const PyroVec3 vym = velocity.values[cellIndex(x, ym, z)];
                const PyroVec3 vyp = velocity.values[cellIndex(x, yp, z)];
                const PyroVec3 vzm = velocity.values[cellIndex(x, y, zm)];
                const PyroVec3 vzp = velocity.values[cellIndex(x, y, zp)];
                divergence.values[idx] = ((vxp.x - vxm.x) + (vyp.y - vym.y) + (vzp.z - vzm.z)) * 0.5f;
            }
        }
    }
}

void PyroSimulation::solvePressure(int iterations) {
    const auto resolution = fields_.resolution();
    auto pressure = fields_.pressureView();
    const auto divergence = fields_.divergenceView();

    for (int i = 0; i < iterations; ++i) {
        for (int z = 0; z < resolution.depth; ++z) {
            for (int y = 0; y < resolution.height; ++y) {
                for (int x = 0; x < resolution.width; ++x) {
                    const auto idx = cellIndex(x, y, z);
                    const int xm = std::max(0, x - 1);
                    const int xp = std::min(resolution.width - 1, x + 1);
                    const int ym = std::max(0, y - 1);
                    const int yp = std::min(resolution.height - 1, y + 1);
                    const int zm = std::max(0, z - 1);
                    const int zp = std::min(resolution.depth - 1, z + 1);
                    const float neighborSum =
                        pressure.values[cellIndex(xm, y, z)] +
                        pressure.values[cellIndex(xp, y, z)] +
                        pressure.values[cellIndex(x, ym, z)] +
                        pressure.values[cellIndex(x, yp, z)] +
                        pressure.values[cellIndex(x, y, zm)] +
                        pressure.values[cellIndex(x, y, zp)];
                    pressure.values[idx] = (divergence.values[idx] + neighborSum) / 6.0f;
                }
            }
        }
    }
}

void PyroSimulation::projectVelocity() {
    const auto resolution = fields_.resolution();
    auto pressure = fields_.pressureView();
    auto velocity = fields_.velocityView();

    for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
            for (int x = 0; x < resolution.width; ++x) {
                const auto idx = cellIndex(x, y, z);
                const int xm = std::max(0, x - 1);
                const int xp = std::min(resolution.width - 1, x + 1);
                const int ym = std::max(0, y - 1);
                const int yp = std::min(resolution.height - 1, y + 1);
                const int zm = std::max(0, z - 1);
                const int zp = std::min(resolution.depth - 1, z + 1);
                const float gradX = pressure.values[cellIndex(xp, y, z)] - pressure.values[cellIndex(xm, y, z)];
                const float gradY = pressure.values[cellIndex(x, yp, z)] - pressure.values[cellIndex(x, ym, z)];
                const float gradZ = pressure.values[cellIndex(x, y, zp)] - pressure.values[cellIndex(x, y, zm)];
                velocity.values[idx].x -= gradX * 0.5f;
                velocity.values[idx].y -= gradY * 0.5f;
                velocity.values[idx].z -= gradZ * 0.5f;
            }
        }
    }
}

PyroFrameSnapshot PyroSimulation::snapshot() const {
    PyroFrameSnapshot snapshot{};
    snapshot.frameIndex = frameIndex_;
    snapshot.timeSeconds = timeSeconds_;
    snapshot.resolution = fields_.resolution();
    snapshot.availableFields = PyroFieldMask::Density | PyroFieldMask::Temperature | PyroFieldMask::Fuel |
        PyroFieldMask::Pressure | PyroFieldMask::Divergence | PyroFieldMask::Velocity;
    snapshot.backend = backend_;
    snapshot.settingsHash = hashSettings(settings_);
    snapshot.deterministic = backend_ == PyroBackendKind::CPUReference;
    snapshot.density = fields_.densityView();
    snapshot.temperature = fields_.temperatureView();
    snapshot.fuel = fields_.fuelView();
    snapshot.pressure = fields_.pressureView();
    snapshot.divergence = fields_.divergenceView();
    snapshot.velocity = fields_.velocityView();
    return snapshot;
}

PyroMemoryEstimate PyroSimulation::estimateMemory() const noexcept { return fields_.estimateMemory(); }

std::string toString(PyroBoundaryMode mode) {
    switch (mode) {
    case PyroBoundaryMode::Open: return "open";
    case PyroBoundaryMode::Closed: return "closed";
    }
    return "unknown";
}

std::string toString(PyroBackendKind kind) {
    switch (kind) {
    case PyroBackendKind::CPUReference: return "cpu-reference";
    case PyroBackendKind::GPUCompute: return "gpu-compute";
    }
    return "unknown";
}

std::string toString(PyroFieldChannel channel) {
    switch (channel) {
    case PyroFieldChannel::Density: return "density";
    case PyroFieldChannel::Temperature: return "temperature";
    case PyroFieldChannel::Fuel: return "fuel";
    case PyroFieldChannel::Pressure: return "pressure";
    case PyroFieldChannel::Divergence: return "divergence";
    case PyroFieldChannel::Velocity: return "velocity";
    case PyroFieldChannel::Color: return "color";
    }
    return "unknown";
}

std::uint64_t hashSettings(const PyroSimulationSettings& settings) noexcept {
    std::uint64_t seed = kFnvOffset;
    hashAppendFloat(seed, settings.sourceDensity);
    hashAppendFloat(seed, settings.sourceTemperature);
    hashAppendFloat(seed, settings.sourceFuel);
    hashAppendFloat(seed, settings.dissipation);
    hashAppendFloat(seed, settings.coolingRate);
    hashAppendFloat(seed, settings.buoyancy);
    hashAppendFloat(seed, settings.vorticity);
    hashAppendFloat(seed, settings.pressureIterations);
    hashAppendFloat(seed, settings.advectionClamp);
    return seed;
}

std::uint64_t estimatePyroMemoryBytes(const PyroResolution& resolution) noexcept {
    if (resolution.width <= 0 || resolution.height <= 0 || resolution.depth <= 0) {
        return 0;
    }
    const std::size_t count = static_cast<std::size_t>(resolution.width)
        * static_cast<std::size_t>(resolution.height)
        * static_cast<std::size_t>(resolution.depth);
    return static_cast<std::uint64_t>(count * (5u * sizeof(float) + sizeof(PyroVec3)));
}

PyroVec3 clampToDomain(const PyroDomain& domain, const PyroVec3& position) noexcept {
    return clampPositionToDomain(domain, position);
}

bool isFinite(const PyroVec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

}
