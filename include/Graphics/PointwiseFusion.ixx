module;
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Graphics.PointwiseFusion;

export namespace ArtifactCore {

enum class PointwiseValueType : std::uint8_t {
    Float1,
    Float2,
    Float3,
    Float4
};

enum class PointwiseOperationKind : std::uint8_t {
    Input,
    Constant,
    Add,
    Multiply,
    Lerp,
    Saturate,
    Select
};

struct PointwiseValue {
    std::uint32_t id = 0;
    PointwiseValueType type = PointwiseValueType::Float4;
};

struct PointwiseOperation {
    std::uint32_t id = 0;
    PointwiseOperationKind kind = PointwiseOperationKind::Input;
    PointwiseValue output{};
    std::vector<PointwiseValue> inputs;
    std::string semantic;
    bool fusible = true;
};

class LIBRARY_DLL_API PointwiseFusionGraph {
public:
    PointwiseValue addInput(std::string semantic, PointwiseValueType type = PointwiseValueType::Float4)
    {
        PointwiseOperation operation;
        operation.id = nextId_++;
        operation.kind = PointwiseOperationKind::Input;
        operation.output = {operation.id, type};
        operation.semantic = std::move(semantic);
        operations_.push_back(operation);
        return operation.output;
    }

    PointwiseValue addOperation(PointwiseOperationKind kind,
                                std::vector<PointwiseValue> inputs,
                                PointwiseValueType outputType = PointwiseValueType::Float4)
    {
        PointwiseOperation operation;
        operation.id = nextId_++;
        operation.kind = kind;
        operation.output = {operation.id, outputType};
        operation.inputs = std::move(inputs);
        operations_.push_back(operation);
        return operation.output;
    }

    bool isValid() const noexcept
    {
        for (const auto& operation : operations_) {
            for (const auto& input : operation.inputs) {
                if (input.id >= nextId_ || input.id == operation.id) {
                    return false;
                }
            }
        }
        return true;
    }

    std::size_t operationCount() const noexcept { return operations_.size(); }
    const std::vector<PointwiseOperation>& operations() const noexcept { return operations_; }
    void clear() noexcept { operations_.clear(); nextId_ = 1; }

private:
    std::vector<PointwiseOperation> operations_;
    std::uint32_t nextId_ = 1;
};

}
