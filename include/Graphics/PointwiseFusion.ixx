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
        outputId_ = operation.id;
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
        outputId_ = operation.id;
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
        outputId_ = operation.id;
        return operation.output;
    }

    bool isValid() const noexcept
    {
        for (const auto& operation : operations_) {
            const auto expectedInputs = [&]() -> std::size_t {
                switch (operation.kind) {
                    case PointwiseOperationKind::Input:
                    case PointwiseOperationKind::Constant: return 0;
                    case PointwiseOperationKind::Saturate: return 1;
                    case PointwiseOperationKind::Add:
                    case PointwiseOperationKind::Multiply: return 2;
                    case PointwiseOperationKind::Lerp:
                    case PointwiseOperationKind::Select: return 3;
                }
                return 0;
            }();
            if (operation.inputs.size() != expectedInputs) return false;

            for (const auto& input : operation.inputs) {
                bool produced = false;
                PointwiseValueType inputType = PointwiseValueType::Float4;
                for (const auto& prior : operations_) {
                    if (prior.id == operation.id) break;
                    if (prior.output.id == input.id) {
                        produced = true;
                        inputType = prior.output.type;
                        break;
                    }
                }
                if (!produced || inputType != input.type) {
                    return false;
                }
            }

            if (operation.kind != PointwiseOperationKind::Input &&
                operation.kind != PointwiseOperationKind::Constant) {
                for (const auto& input : operation.inputs) {
                    if (input.type != operation.output.type) return false;
                }
            }
        }
        return true;
    }

    std::size_t operationCount() const noexcept { return operations_.size(); }
    const std::vector<PointwiseOperation>& operations() const noexcept { return operations_; }
    void setOutput(const PointwiseValue value) noexcept { outputId_ = value.id; }
    PointwiseValue finalValue() const noexcept
    {
        for (const auto& operation : operations_) {
            if (operation.output.id == outputId_) return operation.output;
        }
        return {};
    }

    std::string emitHlslBody() const
    {
        std::string result;
        result.reserve(operations_.size() * 48);
        for (const auto& operation : operations_) {
            const auto outputName = valueName(operation.output.id);
            const auto outputType = typeName(operation.output.type);
            if (operation.kind == PointwiseOperationKind::Input) {
                result += "    " + outputType + " " + outputName + " = " + sanitizeIdentifier(operation.semantic) + ";\n";
                continue;
            }

            if (operation.inputs.empty()) {
                result += "    " + outputType + " " + outputName + " = 0;\n";
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
            result += "    " + outputType + " " + outputName + " = " + expression + ";\n";
        }
        return result;
    }

    std::string emitHlslFunction(std::string functionName, PointwiseValue output) const
    {
        std::string result = typeName(output.type) + " " + sanitizeIdentifier(functionName) + "(";
        bool first = true;
        for (const auto& operation : operations_) {
            if (operation.kind != PointwiseOperationKind::Input) continue;
            if (!first) result += ", ";
            result += typeName(operation.output.type) + " " + sanitizeIdentifier(operation.semantic);
            first = false;
        }
        result += ")\n{\n";
        result += emitHlslBody();
        result += "    return " + valueName(output.id) + ";\n}\n";
        return result;
    }

    void clear() noexcept { operations_.clear(); nextId_ = 1; outputId_ = 0; }

private:
    static const char* typeName(const PointwiseValueType type) noexcept
    {
        switch (type) {
            case PointwiseValueType::Float1: return "float";
            case PointwiseValueType::Float2: return "float2";
            case PointwiseValueType::Float3: return "float3";
            case PointwiseValueType::Float4:
            default: return "float4";
        }
    }

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
    std::uint32_t outputId_ = 0;
};

}
