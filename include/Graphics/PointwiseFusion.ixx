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
    std::string literal;
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

    PointwiseValue addConstant(std::string hlslLiteral,
                               PointwiseValueType type = PointwiseValueType::Float4)
    {
        PointwiseOperation operation;
        operation.id = nextId_++;
        operation.kind = PointwiseOperationKind::Constant;
        operation.output = {operation.id, type};
        operation.literal = std::move(hlslLiteral);
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
                bool produced = false;
                for (const auto& prior : operations_) {
                    if (prior.id == operation.id) break;
                    if (prior.output.id == input.id) {
                        produced = true;
                        break;
                    }
                }
                if (!produced) {
                    return false;
                }
            }
        }
        return true;
    }

    std::size_t operationCount() const noexcept { return operations_.size(); }
    const std::vector<PointwiseOperation>& operations() const noexcept { return operations_; }

    std::string emitHlslBody() const
    {
        std::string result;
        result.reserve(operations_.size() * 48);
        for (const auto& operation : operations_) {
            const auto outputName = valueName(operation.output.id);
            if (operation.kind == PointwiseOperationKind::Input) {
                result += "    float4 " + outputName + " = " + sanitizeIdentifier(operation.semantic) + ";\n";
                continue;
            }

            if (operation.inputs.empty()) {
                result += "    float4 " + outputName + " = 0.0.xxxx;\n";
                continue;
            }

            std::string expression;
            switch (operation.kind) {
                case PointwiseOperationKind::Add:
                    expression = valueName(operation.inputs[0].id);
                    if (operation.inputs.size() > 1) expression += " + " + valueName(operation.inputs[1].id);
                    break;
                case PointwiseOperationKind::Multiply:
                    expression = valueName(operation.inputs[0].id);
                    if (operation.inputs.size() > 1) expression += " * " + valueName(operation.inputs[1].id);
                    break;
                case PointwiseOperationKind::Lerp:
                    if (operation.inputs.size() >= 3) {
                        expression = "lerp(" + valueName(operation.inputs[0].id) + ", " +
                                     valueName(operation.inputs[1].id) + ", " +
                                     valueName(operation.inputs[2].id) + ")";
                    }
                    break;
                case PointwiseOperationKind::Saturate:
                    expression = "saturate(" + valueName(operation.inputs[0].id) + ")";
                    break;
                case PointwiseOperationKind::Select:
                    if (operation.inputs.size() >= 3) {
                        expression = "(" + valueName(operation.inputs[0].id) + " != 0.0.xxxx ? " +
                                     valueName(operation.inputs[1].id) + " : " +
                                     valueName(operation.inputs[2].id) + ")";
                    }
                    break;
                case PointwiseOperationKind::Input:
                    expression = "0.0.xxxx";
                    break;
                case PointwiseOperationKind::Constant:
                    expression = operation.literal.empty() ? "0.0.xxxx" : operation.literal;
                    break;
            }
            if (expression.empty()) expression = "0.0.xxxx";
            result += "    float4 " + outputName + " = " + expression + ";\n";
        }
        return result;
    }

    std::string emitHlslFunction(std::string functionName, PointwiseValue output) const
    {
        std::string result = "float4 " + sanitizeIdentifier(functionName) + "(";
        bool first = true;
        for (const auto& operation : operations_) {
            if (operation.kind != PointwiseOperationKind::Input) continue;
            if (!first) result += ", ";
            result += "float4 " + sanitizeIdentifier(operation.semantic);
            first = false;
        }
        result += ")\n{\n";
        result += emitHlslBody();
        result += "    return " + valueName(output.id) + ";\n}\n";
        return result;
    }

    void clear() noexcept { operations_.clear(); nextId_ = 1; }

private:
    static std::string valueName(const std::uint32_t id)
    {
        return "value_" + std::to_string(id);
    }

    static std::string sanitizeIdentifier(const std::string& value)
    {
        std::string result;
        for (const char character : value) {
            const bool valid = (character >= 'a' && character <= 'z') ||
                               (character >= 'A' && character <= 'Z') ||
                               (character >= '0' && character <= '9') || character == '_';
            result += valid ? character : '_';
        }
        if (result.empty()) result = "input";
        if (result.front() >= '0' && result.front() <= '9') result.insert(result.begin(), '_');
        return result;
    }

    std::vector<PointwiseOperation> operations_;
    std::uint32_t nextId_ = 1;
};

}
