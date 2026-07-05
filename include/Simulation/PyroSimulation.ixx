module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <span>
#include <string>
#include <vector>

import Float3;

export module Core.Simulation.Pyro;

export namespace ArtifactCore {

enum class PyroBoundaryMode : std::uint8_t { Open = 0, Closed = 1 };
enum class PyroBackendKind : std::uint8_t { CPUReference = 0, GPUCompute = 1 };
enum class PyroFieldChannel : std::uint8_t {
    Density = 0, Temperature = 1, Fuel = 2, Pressure = 3, Divergence = 4, Velocity = 5, Color = 6
};

enum class PyroFieldMask : std::uint32_t {
    None = 0,
    Density = 1u << 0,
    Temperature = 1u << 1,
    Fuel = 1u << 2,
    Pressure = 1u << 3,
    Divergence = 1u << 4,
    Velocity = 1u << 5,
    Color = 1u << 6,
    All = 0x7Fu
};

constexpr inline PyroFieldMask operator|(PyroFieldMask lhs, PyroFieldMask rhs) noexcept {
    return static_cast<PyroFieldMask>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr inline PyroFieldMask operator&(PyroFieldMask lhs, PyroFieldMask rhs) noexcept {
    return static_cast<PyroFieldMask>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr inline PyroFieldMask& operator|=(PyroFieldMask& lhs, PyroFieldMask rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

struct PyroVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr PyroVec3() noexcept = default;
    constexpr PyroVec3(float xValue, float yValue, float zValue) noexcept : x(xValue), y(yValue), z(zValue) {}
};

struct PyroResolution {
    int width = 0;
    int height = 0;
    int depth = 0;
};

struct PyroDomain {
    PyroVec3 min{};
    PyroVec3 max{1.0f, 1.0f, 1.0f};
    float voxelSize = 0.1f;
    float fixedTimeStep = 1.0f / 60.0f;
    PyroBoundaryMode boundaryMode = PyroBoundaryMode::Closed;
    std::size_t memoryBudgetBytes = 256ull * 1024ull * 1024ull;

    [[nodiscard]] PyroResolution resolution() const noexcept;
    [[nodiscard]] float width() const noexcept;
    [[nodiscard]] float height() const noexcept;
    [[nodiscard]] float depth() const noexcept;
};

struct PyroMemoryEstimate {
    PyroResolution resolution{};
    std::size_t densityBytes = 0;
    std::size_t temperatureBytes = 0;
    std::size_t fuelBytes = 0;
    std::size_t pressureBytes = 0;
    std::size_t divergenceBytes = 0;
    std::size_t velocityBytes = 0;
    std::size_t colorBytes = 0;
    std::size_t totalBytes = 0;
};

struct PyroFieldSnapshot {
    PyroResolution resolution{};
    std::vector<float> density;
    std::vector<float> temperature;
    std::vector<float> fuel;
    std::vector<float> pressure;
    std::vector<float> divergence;
    std::vector<PyroVec3> velocity;
};

struct PyroCacheStatus {
    bool supported = false;
    bool loaded = false;
    std::string reason;
};

struct PyroSampleResult {
    float scalar = 0.0f;
    PyroVec3 vector{};
};

struct PyroScalarFieldView {
    std::span<float> values;
    PyroResolution resolution{};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] float& at(int x, int y, int z) const;
};

struct PyroConstScalarFieldView {
    std::span<const float> values;
    PyroResolution resolution{};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const float& at(int x, int y, int z) const;
};

struct PyroVectorFieldView {
    std::span<PyroVec3> values;
    PyroResolution resolution{};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] PyroVec3& at(int x, int y, int z) const;
};

struct PyroConstVectorFieldView {
    std::span<const PyroVec3> values;
    PyroResolution resolution{};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const PyroVec3& at(int x, int y, int z) const;
};

struct PyroSimulationSettings {
    float sourceDensity = 1.0f;
    float sourceTemperature = 1.0f;
    float sourceFuel = 1.0f;
    float dissipation = 0.01f;
    float coolingRate = 0.1f;
    float buoyancy = 0.5f;
    float vorticity = 0.0f;
    float pressureIterations = 20.0f;
    float advectionClamp = 1.0f;
};

struct PyroSourceState {
    PyroVec3 position{};
    PyroVec3 extent{0.5f, 0.5f, 0.5f};
    PyroVec3 velocity{};
    float density = 0.0f;
    float temperature = 0.0f;
    float fuel = 0.0f;
    bool enabled = true;
};

enum class PyroColliderType : std::uint8_t { Box = 0, Sphere = 1 };

struct PyroColliderState {
    PyroColliderType type = PyroColliderType::Box;
    PyroVec3 center{};
    PyroVec3 extent{0.5f, 0.5f, 0.5f};
    PyroVec3 velocity{};
    bool enabled = true;
    bool noSlip = true;
};

struct PyroFrameInput {
    std::uint64_t frameIndex = 0;
    double timeSeconds = 0.0;
    double deltaSeconds = 0.0;
    std::uint64_t settingsHash = 0;
    std::span<const PyroSourceState> sources{};
    std::span<const PyroColliderState> colliders{};
};

struct PyroFrameSnapshot {
    std::uint64_t frameIndex = 0;
    double timeSeconds = 0.0;
    PyroResolution resolution{};
    PyroFieldMask availableFields = PyroFieldMask::None;
    PyroBackendKind backend = PyroBackendKind::CPUReference;
    std::uint64_t settingsHash = 0;
    bool deterministic = true;
    PyroConstScalarFieldView density{};
    PyroConstScalarFieldView temperature{};
    PyroConstScalarFieldView fuel{};
    PyroConstScalarFieldView pressure{};
    PyroConstScalarFieldView divergence{};
    PyroConstVectorFieldView velocity{};
};

class PyroFieldSet {
public:
    void resize(PyroResolution resolution);
    void clear();
    [[nodiscard]] PyroResolution resolution() const noexcept { return resolution_; }
    [[nodiscard]] std::size_t cellCount() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return cellCount() == 0; }
    [[nodiscard]] PyroScalarFieldView densityView() noexcept;
    [[nodiscard]] PyroConstScalarFieldView densityView() const noexcept;
    [[nodiscard]] PyroScalarFieldView temperatureView() noexcept;
    [[nodiscard]] PyroConstScalarFieldView temperatureView() const noexcept;
    [[nodiscard]] PyroScalarFieldView fuelView() noexcept;
    [[nodiscard]] PyroConstScalarFieldView fuelView() const noexcept;
    [[nodiscard]] PyroScalarFieldView pressureView() noexcept;
    [[nodiscard]] PyroConstScalarFieldView pressureView() const noexcept;
    [[nodiscard]] PyroScalarFieldView divergenceView() noexcept;
    [[nodiscard]] PyroConstScalarFieldView divergenceView() const noexcept;
    [[nodiscard]] PyroVectorFieldView velocityView() noexcept;
    [[nodiscard]] PyroConstVectorFieldView velocityView() const noexcept;
    [[nodiscard]] std::vector<float>& densityStorage() noexcept { return density_; }
    [[nodiscard]] const std::vector<float>& densityStorage() const noexcept { return density_; }
    [[nodiscard]] std::vector<float>& temperatureStorage() noexcept { return temperature_; }
    [[nodiscard]] const std::vector<float>& temperatureStorage() const noexcept { return temperature_; }
    [[nodiscard]] std::vector<float>& fuelStorage() noexcept { return fuel_; }
    [[nodiscard]] const std::vector<float>& fuelStorage() const noexcept { return fuel_; }
    [[nodiscard]] std::vector<float>& pressureStorage() noexcept { return pressure_; }
    [[nodiscard]] const std::vector<float>& pressureStorage() const noexcept { return pressure_; }
    [[nodiscard]] std::vector<float>& divergenceStorage() noexcept { return divergence_; }
    [[nodiscard]] const std::vector<float>& divergenceStorage() const noexcept { return divergence_; }
    [[nodiscard]] std::vector<PyroVec3>& velocityStorage() noexcept { return velocity_; }
    [[nodiscard]] const std::vector<PyroVec3>& velocityStorage() const noexcept { return velocity_; }
    [[nodiscard]] PyroMemoryEstimate estimateMemory() const noexcept;
    [[nodiscard]] float sampleScalar(const PyroConstScalarFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept;
    [[nodiscard]] PyroVec3 sampleVector(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept;
    [[nodiscard]] PyroVec3 gradient(const PyroConstScalarFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept;
    [[nodiscard]] float divergence(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept;
    [[nodiscard]] float curlMagnitude(const PyroConstVectorFieldView& view, const PyroVec3& position, PyroBoundaryMode boundaryMode) const noexcept;

private:
    PyroResolution resolution_{};
    std::vector<float> density_;
    std::vector<float> temperature_;
    std::vector<float> fuel_;
    std::vector<float> pressure_;
    std::vector<float> divergence_;
    std::vector<PyroVec3> velocity_;

    friend class PyroSimulation;
};

class PyroSimulation {
public:
    PyroSimulation() = default;
    explicit PyroSimulation(PyroDomain domain);

    void setDomain(PyroDomain domain);
    [[nodiscard]] const PyroDomain& domain() const noexcept { return domain_; }
    void setSettings(PyroSimulationSettings settings);
    [[nodiscard]] const PyroSimulationSettings& settings() const noexcept { return settings_; }
    void setBackend(PyroBackendKind backend) noexcept { backend_ = backend; }
    [[nodiscard]] PyroBackendKind backend() const noexcept { return backend_; }
    void setPaused(bool paused) noexcept { paused_ = paused; }
    [[nodiscard]] bool isPaused() const noexcept { return paused_; }
    void setFixedTimeStep(double fixedTimeStep) noexcept;
    [[nodiscard]] double fixedTimeStep() const noexcept { return fixedTimeStep_; }
    void setCacheInterval(std::uint64_t cacheInterval) noexcept;
    [[nodiscard]] std::uint64_t cacheInterval() const noexcept { return cacheInterval_; }
    void setCacheDirectory(std::filesystem::path cacheDirectory);
    [[nodiscard]] const std::filesystem::path& cacheDirectory() const noexcept { return cacheDirectory_; }
    [[nodiscard]] PyroCacheStatus saveCheckpointToDisk(std::uint64_t frameIndex) const;
    [[nodiscard]] PyroCacheStatus loadCheckpointFromDisk(std::uint64_t frameIndex);
    void reset();
    void seek(std::uint64_t frameIndex, double timeSeconds);
    void step(double deltaSeconds);
    [[nodiscard]] const PyroFieldSet& fields() const noexcept { return fields_; }
    [[nodiscard]] PyroFieldSet& fields() noexcept { return fields_; }
    [[nodiscard]] PyroFrameSnapshot snapshot() const;
    [[nodiscard]] PyroMemoryEstimate estimateMemory() const noexcept;
    void setSources(std::span<const PyroSourceState> sources);
    [[nodiscard]] std::span<const PyroSourceState> sources() const noexcept { return sources_; }
    void setColliders(std::span<const PyroColliderState> colliders);
    [[nodiscard]] std::span<const PyroColliderState> colliders() const noexcept { return colliders_; }
    [[nodiscard]] std::uint64_t frameIndex() const noexcept { return frameIndex_; }
    [[nodiscard]] double timeSeconds() const noexcept { return timeSeconds_; }

private:
    PyroDomain domain_{};
    PyroSimulationSettings settings_{};
    PyroFieldSet fields_{};
    PyroBackendKind backend_ = PyroBackendKind::CPUReference;
    bool paused_ = false;
    std::uint64_t frameIndex_ = 0;
    double timeSeconds_ = 0.0;
    std::vector<PyroSourceState> ownedSources_;
    std::span<const PyroSourceState> sources_{};
    std::vector<PyroColliderState> ownedColliders_;
    std::span<const PyroColliderState> colliders_{};
    std::unordered_map<std::uint64_t, PyroFieldSnapshot> checkpointCache_;
    double fixedTimeStep_ = 1.0 / 60.0;
    double accumulator_ = 0.0;
    std::uint64_t cacheInterval_ = 16;
    std::filesystem::path cacheDirectory_;

    [[nodiscard]] std::size_t cellIndex(int x, int y, int z) const noexcept;
    [[nodiscard]] bool isInsideDomain(const PyroVec3& position) const noexcept;
    [[nodiscard]] bool isInsideCollider(const PyroColliderState& collider, const PyroVec3& position) const noexcept;
    void applyColliders();
    void applyCombustion(float deltaSeconds);
    void applyVorticityConfinement(float deltaSeconds);
    void integrateStep(float deltaSeconds);
    void storeCheckpoint();
    bool saveCheckpointSnapshot(const PyroFieldSnapshot& snapshot, const std::filesystem::path& path) const;
    bool loadCheckpointSnapshot(PyroFieldSnapshot& snapshot, const std::filesystem::path& path) const;
    bool restoreCheckpoint(std::uint64_t frameIndex);
    void computeDivergence();
    void solvePressure(int iterations);
    void projectVelocity();
};

[[nodiscard]] std::string toString(PyroBoundaryMode mode);
[[nodiscard]] std::string toString(PyroBackendKind kind);
[[nodiscard]] std::string toString(PyroFieldChannel channel);
[[nodiscard]] std::uint64_t hashSettings(const PyroSimulationSettings& settings) noexcept;
[[nodiscard]] std::uint64_t estimatePyroMemoryBytes(const PyroResolution& resolution) noexcept;
[[nodiscard]] PyroVec3 clampToDomain(const PyroDomain& domain, const PyroVec3& position) noexcept;
[[nodiscard]] bool isFinite(const PyroVec3& value) noexcept;

}
