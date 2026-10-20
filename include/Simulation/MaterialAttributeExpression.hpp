#pragma once

#include "MaterialAttributes.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>

namespace ArtifactCore::Experimental::MaterialTransfer {

inline constexpr std::size_t kFloatExpressionMaxBytes = 256;
inline constexpr std::size_t kFloatExpressionMaxInstructions = 64;
inline constexpr std::size_t kFloatExpressionMaxStack = 64;
inline constexpr std::size_t kFloatExpressionMaxNesting = 24;
inline constexpr std::size_t kFloatExpressionMaxBindings = 128;
inline constexpr std::size_t kFloatExpressionMaxNameBytes = 64;
inline constexpr std::size_t kFloatExpressionSourceHeaderBytes = 20;
inline constexpr std::size_t kFloatExpressionSourceMaxRecordBytes =
    kFloatExpressionSourceHeaderBytes + kFloatExpressionMaxBytes;

enum class FloatExpressionStatus : std::uint8_t {
    Ready, InvalidExpression, UnknownAttribute, TypeOrUnitMismatch,
    InvalidInput, CapacityExceeded, Defer, DivisionByZero
};

enum class FloatExpressionOpcode : std::uint8_t {
    Constant, Attribute, Negate, Add, Subtract, Multiply, Divide,
    Equal, NotEqual, Less, LessOrEqual, Greater, GreaterOrEqual,
    Abs, LogicalNot, Minimum, Maximum, Clamp, ToBoolean,
    Jump, JumpIfFalse, JumpIfTrue
};

struct FloatAttributeExpressionBinding {
    std::string_view name;
    std::span<const float> values;
    AttributeUnit unit = AttributeUnit::Dimensionless;
};

struct FloatAttributeExpressionInstruction {
    FloatExpressionOpcode opcode = FloatExpressionOpcode::Constant;
    float constant = 0.0f;
    std::span<const float> values;
    std::uint8_t jumpTarget = 0;
};

// Compiled RPN program; attribute spans are borrowed from the schema supplied
// at compile time and must remain alive and unchanged through evaluation.
struct FloatAttributeExpression {
    std::array<FloatAttributeExpressionInstruction,
               kFloatExpressionMaxInstructions> instructions{};
    std::uint8_t instructionCount = 0;
    std::uint8_t maxStackDepth = 0;
    std::size_t elementCount = 0;
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit resultUnit = AttributeUnit::Dimensionless;
};

// Persistable source form for one named attribute assignment. The expression
// bytes are fixed-capacity ASCII; compiled programs remain borrowed runtime data.
struct FloatAttributeExpressionSource {
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit targetUnit = AttributeUnit::Dimensionless;
    std::uint16_t expressionLength = 0;
    std::array<char, kFloatExpressionMaxBytes> expression{};

    std::string_view text() const noexcept {
        if (expressionLength > expression.size()) return {};
        return {expression.data(), expressionLength};
    }
};

enum class FloatExpressionSourceCodecStatus : std::uint8_t {
    Ready, InvalidInput, Defer, UnsupportedVersion
};

namespace expression_detail {
inline bool nameStart(char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value == '_';
}
inline bool nameContinue(char value) noexcept {
    return nameStart(value) || (value >= '0' && value <= '9');
}
inline bool validUnit(AttributeUnit unit) noexcept {
    return static_cast<unsigned>(unit) <= static_cast<unsigned>(AttributeUnit::Meters);
}
inline bool validOwner(AttributeOwner owner) noexcept {
    return static_cast<unsigned>(owner) <= static_cast<unsigned>(AttributeOwner::Layer);
}
inline bool validSourceRecord(const FloatAttributeExpressionSource& source) noexcept {
    if (source.targetKey == 0 || !validOwner(source.targetOwner) ||
        !validUnit(source.targetUnit) || source.expressionLength == 0 ||
        source.expressionLength > source.expression.size()) return false;
    for (std::size_t i = 0; i < source.expressionLength; ++i) {
        const auto value = static_cast<unsigned char>(source.expression[i]);
        if (value == 0 || value > 0x7e ||
            (value < 0x20 && value != '\t' && value != '\r' && value != '\n'))
            return false;
    }
    return true;
}
inline void writeSourceU16(std::span<std::byte> bytes, std::size_t offset,
                           std::uint16_t value) noexcept {
    for (std::size_t i = 0; i < 2; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline void writeSourceU32(std::span<std::byte> bytes, std::size_t offset,
                           std::uint32_t value) noexcept {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline void writeSourceU64(std::span<std::byte> bytes, std::size_t offset,
                           std::uint64_t value) noexcept {
    for (std::size_t i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline std::uint16_t readSourceU16(std::span<const std::byte> bytes,
                                   std::size_t offset) noexcept {
    std::uint16_t value = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto part = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(bytes[offset + i]));
        value = static_cast<std::uint16_t>(value | static_cast<std::uint16_t>(part << (i * 8)));
    }
    return value;
}
inline std::uint32_t readSourceU32(std::span<const std::byte> bytes,
                                   std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}
inline std::uint64_t readSourceU64(std::span<const std::byte> bytes,
                                   std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}

struct TypeInfo {
    AttributeUnit unit = AttributeUnit::Dimensionless;
    bool literalOnly = true;
};

class Parser {
public:
    Parser(std::string_view source,
           std::span<const FloatAttributeExpressionBinding> bindings,
           std::size_t elementCount,
           FloatAttributeExpression& output) noexcept
        : source_(source), bindings_(bindings), elementCount_(elementCount), output_(output) {
        output_.elementCount = elementCount;
    }

    FloatExpressionStatus parse(TypeInfo& result) noexcept {
        if (!parseConditional(0, result)) return status_;
        skipWhitespace();
        if (cursor_ != source_.size() || stackDepth_ != 1)
            return FloatExpressionStatus::InvalidExpression;
        output_.instructionCount = static_cast<std::uint8_t>(instructionCount_);
        output_.maxStackDepth = static_cast<std::uint8_t>(maxStackDepth_);
        return FloatExpressionStatus::Ready;
    }

private:
    void skipWhitespace() noexcept {
        while (cursor_ < source_.size() &&
               (source_[cursor_] == ' ' || source_[cursor_] == '\t' ||
                source_[cursor_] == '\r' || source_[cursor_] == '\n'))
            ++cursor_;
    }

    bool append(FloatAttributeExpressionInstruction instruction,
                bool unary) noexcept {
        if (instructionCount_ >= kFloatExpressionMaxInstructions) {
            status_ = FloatExpressionStatus::CapacityExceeded;
            return false;
        }
        if (unary) {
            if (stackDepth_ < 1) {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
        } else if (instruction.opcode == FloatExpressionOpcode::Jump) {
            // Unconditional forward jump has no stack effect.
        } else if (instruction.opcode == FloatExpressionOpcode::JumpIfFalse ||
                   instruction.opcode == FloatExpressionOpcode::JumpIfTrue) {
            if (stackDepth_ < 1) {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            --stackDepth_;
        } else if (instruction.opcode == FloatExpressionOpcode::Add ||
                   instruction.opcode == FloatExpressionOpcode::Subtract ||
                   instruction.opcode == FloatExpressionOpcode::Multiply ||
                   instruction.opcode == FloatExpressionOpcode::Divide ||
                   instruction.opcode == FloatExpressionOpcode::Equal ||
                   instruction.opcode == FloatExpressionOpcode::NotEqual ||
                   instruction.opcode == FloatExpressionOpcode::Less ||
                   instruction.opcode == FloatExpressionOpcode::LessOrEqual ||
                   instruction.opcode == FloatExpressionOpcode::Greater ||
                   instruction.opcode == FloatExpressionOpcode::GreaterOrEqual ||
                   instruction.opcode == FloatExpressionOpcode::Minimum ||
                   instruction.opcode == FloatExpressionOpcode::Maximum) {
            if (stackDepth_ < 2) {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            --stackDepth_;
        } else if (instruction.opcode == FloatExpressionOpcode::Clamp) {
            if (stackDepth_ < 3) {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            stackDepth_ -= 2;
        } else {
            ++stackDepth_;
            if (stackDepth_ > kFloatExpressionMaxStack) {
                status_ = FloatExpressionStatus::CapacityExceeded;
                return false;
            }
            if (stackDepth_ > maxStackDepth_) maxStackDepth_ = stackDepth_;
        }
        output_.instructions[instructionCount_++] = instruction;
        return true;
    }

    bool patchJump(std::size_t instructionIndex, std::size_t target) noexcept {
        if (instructionIndex >= instructionCount_ || target > instructionCount_ ||
            target > kFloatExpressionMaxInstructions) {
            status_ = FloatExpressionStatus::CapacityExceeded;
            return false;
        }
        output_.instructions[instructionIndex].jumpTarget =
            static_cast<std::uint8_t>(target);
        return true;
    }

    bool parseConditional(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo condition;
        if (!parseLogicalOr(depth, condition)) return false;
        skipWhitespace();
        if (cursor_ >= source_.size() || source_[cursor_] != '?') {
            result = condition;
            return true;
        }
        if (depth >= kFloatExpressionMaxNesting) {
            status_ = FloatExpressionStatus::CapacityExceeded;
            return false;
        }
        if (condition.unit != AttributeUnit::Dimensionless) {
            status_ = FloatExpressionStatus::TypeOrUnitMismatch;
            return false;
        }
        ++cursor_;
        const auto baseDepth = stackDepth_ - 1;
        const auto branch = instructionCount_;
        if (!append({FloatExpressionOpcode::JumpIfFalse}, false)) return false;
        TypeInfo whenTrue;
        if (!parseConditional(depth + 1, whenTrue)) return false;
        skipWhitespace();
        if (cursor_ >= source_.size() || source_[cursor_] != ':') {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        ++cursor_;
        const auto finish = instructionCount_;
        if (!append({FloatExpressionOpcode::Jump}, false)) return false;
        const auto falseBranch = instructionCount_;
        if (!patchJump(branch, falseBranch)) return false;
        stackDepth_ = baseDepth;
        TypeInfo whenFalse;
        if (!parseConditional(depth + 1, whenFalse)) return false;
        if (stackDepth_ != baseDepth + 1 || !combineTypes(whenTrue, whenFalse, result))
            return false;
        if (!patchJump(finish, instructionCount_)) return false;
        return true;
    }

    bool parseLogicalOr(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo left;
        if (!parseLogicalAnd(depth, left)) return false;
        for (;;) {
            skipWhitespace();
            if (cursor_ + 1 >= source_.size() || source_[cursor_] != '|' ||
                source_[cursor_ + 1] != '|') break;
            if (left.unit != AttributeUnit::Dimensionless) {
                status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                return false;
            }
            cursor_ += 2;
            const auto baseDepth = stackDepth_ - 1;
            const auto trueBranchJump = instructionCount_;
            if (!append({FloatExpressionOpcode::JumpIfTrue}, false)) return false;
            TypeInfo right;
            if (!parseLogicalAnd(depth, right)) return false;
            if (right.unit != AttributeUnit::Dimensionless) {
                status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                return false;
            }
            if (!append({FloatExpressionOpcode::ToBoolean}, true)) return false;
            const auto finish = instructionCount_;
            if (!append({FloatExpressionOpcode::Jump}, false)) return false;
            const auto trueBranch = instructionCount_;
            if (!patchJump(trueBranchJump, trueBranch)) return false;
            stackDepth_ = baseDepth;
            if (!append({FloatExpressionOpcode::Constant, 1.0f}, false)) return false;
            if (!patchJump(finish, instructionCount_)) return false;
            left = {AttributeUnit::Dimensionless, false};
        }
        result = left;
        return true;
    }

    bool parseLogicalAnd(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo left;
        if (!parseComparison(depth, left)) return false;
        for (;;) {
            skipWhitespace();
            if (cursor_ + 1 >= source_.size() || source_[cursor_] != '&' ||
                source_[cursor_ + 1] != '&') break;
            if (left.unit != AttributeUnit::Dimensionless) {
                status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                return false;
            }
            cursor_ += 2;
            const auto baseDepth = stackDepth_ - 1;
            const auto falseBranchJump = instructionCount_;
            if (!append({FloatExpressionOpcode::JumpIfFalse}, false)) return false;
            TypeInfo right;
            if (!parseComparison(depth, right)) return false;
            if (right.unit != AttributeUnit::Dimensionless) {
                status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                return false;
            }
            if (!append({FloatExpressionOpcode::ToBoolean}, true)) return false;
            const auto finish = instructionCount_;
            if (!append({FloatExpressionOpcode::Jump}, false)) return false;
            const auto falseBranch = instructionCount_;
            if (!patchJump(falseBranchJump, falseBranch)) return false;
            stackDepth_ = baseDepth;
            if (!append({FloatExpressionOpcode::Constant, 0.0f}, false)) return false;
            if (!patchJump(finish, instructionCount_)) return false;
            left = {AttributeUnit::Dimensionless, false};
        }
        result = left;
        return true;
    }

    bool parseExpression(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo left;
        if (!parseTerm(depth, left)) return false;
        for (;;) {
            skipWhitespace();
            if (cursor_ >= source_.size() ||
                (source_[cursor_] != '+' && source_[cursor_] != '-')) break;
            const char symbol = source_[cursor_++];
            TypeInfo right;
            if (!parseTerm(depth, right)) return false;
            TypeInfo combined;
            if (left.literalOnly && !right.literalOnly) combined.unit = right.unit;
            else if (right.literalOnly && !left.literalOnly) combined.unit = left.unit;
            else if (left.unit == right.unit) combined.unit = left.unit;
            else {
                status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                return false;
            }
            combined.literalOnly = left.literalOnly && right.literalOnly;
            const auto opcode = symbol == '+' ? FloatExpressionOpcode::Add
                                               : FloatExpressionOpcode::Subtract;
            if (!append({opcode}, false)) return false;
            left = combined;
        }
        result = left;
        return true;
    }

    bool parseComparison(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo left;
        if (!parseExpression(depth, left)) return false;
        skipWhitespace();
        if (cursor_ >= source_.size()) {
            result = left;
            return true;
        }
        const char first = source_[cursor_];
        FloatExpressionOpcode opcode{};
        std::size_t tokenLength = 0;
        if (first == '<' || first == '>') {
            tokenLength = 1;
            if (cursor_ + 1 < source_.size() && source_[cursor_ + 1] == '=') {
                tokenLength = 2;
                opcode = first == '<' ? FloatExpressionOpcode::LessOrEqual
                                       : FloatExpressionOpcode::GreaterOrEqual;
            } else {
                opcode = first == '<' ? FloatExpressionOpcode::Less
                                       : FloatExpressionOpcode::Greater;
            }
        } else if (first == '=' || first == '!') {
            if (cursor_ + 1 >= source_.size() || source_[cursor_ + 1] != '=') {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            tokenLength = 2;
            opcode = first == '=' ? FloatExpressionOpcode::Equal
                                  : FloatExpressionOpcode::NotEqual;
        } else {
            result = left;
            return true;
        }
        cursor_ += tokenLength;
        TypeInfo right;
        if (!parseExpression(depth, right)) return false;
        if (!left.literalOnly && !right.literalOnly && left.unit != right.unit) {
            status_ = FloatExpressionStatus::TypeOrUnitMismatch;
            return false;
        }
        if (!append({opcode}, false)) return false;
        result = {AttributeUnit::Dimensionless, false};
        return true;
    }

    bool parseTerm(std::size_t depth, TypeInfo& result) noexcept {
        TypeInfo left;
        if (!parseUnary(depth, left)) return false;
        for (;;) {
            skipWhitespace();
            if (cursor_ >= source_.size() ||
                (source_[cursor_] != '*' && source_[cursor_] != '/')) break;
            const char symbol = source_[cursor_++];
            TypeInfo right;
            if (!parseUnary(depth, right)) return false;
            TypeInfo combined;
            if (symbol == '*') {
                if (left.unit == AttributeUnit::Dimensionless) combined.unit = right.unit;
                else if (right.unit == AttributeUnit::Dimensionless) combined.unit = left.unit;
                else {
                    status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                    return false;
                }
            } else {
                if (right.unit == AttributeUnit::Dimensionless) combined.unit = left.unit;
                else if (!left.literalOnly && !right.literalOnly && left.unit == right.unit)
                    combined.unit = AttributeUnit::Dimensionless;
                else {
                    status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                    return false;
                }
            }
            combined.literalOnly = left.literalOnly && right.literalOnly;
            const auto opcode = symbol == '*' ? FloatExpressionOpcode::Multiply
                                               : FloatExpressionOpcode::Divide;
            if (!append({opcode}, false)) return false;
            left = combined;
        }
        result = left;
        return true;
    }

    bool parseUnary(std::size_t depth, TypeInfo& result) noexcept {
        skipWhitespace();
        if (cursor_ < source_.size() &&
            (source_[cursor_] == '+' || source_[cursor_] == '-' || source_[cursor_] == '!')) {
            if (depth >= kFloatExpressionMaxNesting) {
                status_ = FloatExpressionStatus::CapacityExceeded;
                return false;
            }
            const char sign = source_[cursor_++];
            if (sign == '!' && cursor_ < source_.size() && source_[cursor_] == '=') {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            if (!parseUnary(depth + 1, result)) return false;
            if (sign == '-' && !append({FloatExpressionOpcode::Negate}, true)) return false;
            if (sign == '!') {
                if (result.unit != AttributeUnit::Dimensionless) {
                    status_ = FloatExpressionStatus::TypeOrUnitMismatch;
                    return false;
                }
                if (!append({FloatExpressionOpcode::LogicalNot}, true)) return false;
                result = {AttributeUnit::Dimensionless, false};
            }
            return true;
        }
        return parsePrimary(depth, result);
    }

    bool parsePrimary(std::size_t depth, TypeInfo& result) noexcept {
        skipWhitespace();
        if (cursor_ >= source_.size()) {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        if (source_[cursor_] == '(') {
            if (depth >= kFloatExpressionMaxNesting) {
                status_ = FloatExpressionStatus::CapacityExceeded;
                return false;
            }
            ++cursor_;
            if (!parseConditional(depth + 1, result)) return false;
            skipWhitespace();
            if (cursor_ >= source_.size() || source_[cursor_] != ')') {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            ++cursor_;
            return true;
        }
        if (source_[cursor_] == '@') return parseAttribute(result);
        if (nameStart(source_[cursor_])) return parseFunction(depth, result);
        return parseNumber(result);
    }

    bool parseFunction(std::size_t depth, TypeInfo& result) noexcept {
        if (depth >= kFloatExpressionMaxNesting) {
            status_ = FloatExpressionStatus::CapacityExceeded;
            return false;
        }
        const auto start = cursor_++;
        while (cursor_ < source_.size() && nameContinue(source_[cursor_])) ++cursor_;
        const auto name = source_.substr(start, cursor_ - start);
        const bool isAbs = name == "abs";
        const bool isMin = name == "min";
        const bool isMax = name == "max";
        const bool isClamp = name == "clamp";
        if (!isAbs && !isMin && !isMax && !isClamp) {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        skipWhitespace();
        if (cursor_ >= source_.size() || source_[cursor_] != '(') {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        ++cursor_;
        TypeInfo first;
        if (!parseConditional(depth + 1, first)) return false;
        skipWhitespace();
        if (isAbs) {
            if (cursor_ >= source_.size() || source_[cursor_] != ')') {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            ++cursor_;
            if (!append({FloatExpressionOpcode::Abs}, true)) return false;
            result = first;
            return true;
        }

        if (cursor_ >= source_.size() || source_[cursor_] != ',') {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        ++cursor_;
        TypeInfo second;
        if (!parseConditional(depth + 1, second)) return false;
        TypeInfo combined;
        if (!combineTypes(first, second, combined)) return false;
        if (isClamp) {
            skipWhitespace();
            if (cursor_ >= source_.size() || source_[cursor_] != ',') {
                status_ = FloatExpressionStatus::InvalidExpression;
                return false;
            }
            ++cursor_;
            TypeInfo third;
            if (!parseConditional(depth + 1, third)) return false;
            if (!combineTypes(combined, third, combined)) return false;
        }
        skipWhitespace();
        if (cursor_ >= source_.size() || source_[cursor_] != ')') {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        ++cursor_;
        const auto opcode = isMin ? FloatExpressionOpcode::Minimum
            : isMax ? FloatExpressionOpcode::Maximum : FloatExpressionOpcode::Clamp;
        if (!append({opcode}, false)) return false;
        result = combined;
        return true;
    }

    bool combineTypes(const TypeInfo& left, const TypeInfo& right,
                      TypeInfo& combined) noexcept {
        if (left.literalOnly && !right.literalOnly) combined.unit = right.unit;
        else if (right.literalOnly && !left.literalOnly) combined.unit = left.unit;
        else if (left.unit == right.unit) combined.unit = left.unit;
        else {
            status_ = FloatExpressionStatus::TypeOrUnitMismatch;
            return false;
        }
        combined.literalOnly = left.literalOnly && right.literalOnly;
        return true;
    }

    bool parseAttribute(TypeInfo& result) noexcept {
        ++cursor_;
        const auto nameStartOffset = cursor_;
        if (cursor_ >= source_.size() || !nameStart(source_[cursor_])) {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        ++cursor_;
        while (cursor_ < source_.size() && nameContinue(source_[cursor_])) ++cursor_;
        const auto name = source_.substr(nameStartOffset, cursor_ - nameStartOffset);
        const FloatAttributeExpressionBinding* match = nullptr;
        for (const auto& binding : bindings_) {
            if (binding.name != name) continue;
            if (match) {
                status_ = FloatExpressionStatus::InvalidInput;
                return false;
            }
            match = &binding;
        }
        if (!match) {
            status_ = FloatExpressionStatus::UnknownAttribute;
            return false;
        }
        if (!validUnit(match->unit) || match->values.size() != elementCount_) {
            status_ = FloatExpressionStatus::TypeOrUnitMismatch;
            return false;
        }
        if (!append({FloatExpressionOpcode::Attribute, 0.0f, match->values}, false))
            return false;
        result = {match->unit, false};
        return true;
    }

    bool parseNumber(TypeInfo& result) noexcept {
        const char* first = source_.data() + cursor_;
        const char* last = source_.data() + source_.size();
        float value = 0.0f;
        const auto parsed = std::from_chars(first, last, value, std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr == first || !std::isfinite(value)) {
            status_ = FloatExpressionStatus::InvalidExpression;
            return false;
        }
        cursor_ += static_cast<std::size_t>(parsed.ptr - first);
        if (!append({FloatExpressionOpcode::Constant, value}, false)) return false;
        result = {AttributeUnit::Dimensionless, true};
        return true;
    }

    std::string_view source_;
    std::span<const FloatAttributeExpressionBinding> bindings_;
    std::size_t elementCount_ = 0;
    FloatAttributeExpression& output_;
    std::size_t cursor_ = 0;
    std::size_t instructionCount_ = 0;
    std::size_t stackDepth_ = 0;
    std::size_t maxStackDepth_ = 0;
    FloatExpressionStatus status_ = FloatExpressionStatus::InvalidExpression;
};

inline bool validProgram(const FloatAttributeExpression& expression) noexcept {
    if (expression.instructionCount == 0 ||
        expression.instructionCount > kFloatExpressionMaxInstructions ||
        expression.maxStackDepth == 0 ||
        expression.maxStackDepth > kFloatExpressionMaxStack ||
        !validOwner(expression.targetOwner) ||
        !validUnit(expression.resultUnit)) return false;
    std::array<std::int16_t, kFloatExpressionMaxInstructions + 1> depths{};
    depths.fill(-1);
    depths[0] = 0;
    std::size_t maximum = 0;
    const auto mergeDepth = [&](std::size_t target, std::size_t depth) noexcept {
        if (target > expression.instructionCount || depth > kFloatExpressionMaxStack)
            return false;
        auto& existing = depths[target];
        if (existing >= 0) return static_cast<std::size_t>(existing) == depth;
        existing = static_cast<std::int16_t>(depth);
        return true;
    };
    for (std::size_t pc = 0; pc < expression.instructionCount; ++pc) {
        if (depths[pc] < 0) continue;
        std::size_t depth = static_cast<std::size_t>(depths[pc]);
        if (depth > maximum) maximum = depth;
        const auto& instruction = expression.instructions[pc];
        switch (instruction.opcode) {
        case FloatExpressionOpcode::Constant:
            if (!std::isfinite(instruction.constant)) return false;
            ++depth;
            break;
        case FloatExpressionOpcode::Attribute:
            if (instruction.values.size() != expression.elementCount) return false;
            ++depth;
            break;
        case FloatExpressionOpcode::Negate:
        case FloatExpressionOpcode::Abs:
        case FloatExpressionOpcode::LogicalNot:
        case FloatExpressionOpcode::ToBoolean:
            if (depth < 1) return false;
            break;
        case FloatExpressionOpcode::Add:
        case FloatExpressionOpcode::Subtract:
        case FloatExpressionOpcode::Multiply:
        case FloatExpressionOpcode::Divide:
        case FloatExpressionOpcode::Equal:
        case FloatExpressionOpcode::NotEqual:
        case FloatExpressionOpcode::Less:
        case FloatExpressionOpcode::LessOrEqual:
        case FloatExpressionOpcode::Greater:
        case FloatExpressionOpcode::GreaterOrEqual:
        case FloatExpressionOpcode::Minimum:
        case FloatExpressionOpcode::Maximum:
            if (depth < 2) return false;
            --depth;
            break;
        case FloatExpressionOpcode::Clamp:
            if (depth < 3) return false;
            depth -= 2;
            break;
        case FloatExpressionOpcode::Jump:
            if (instruction.jumpTarget <= pc ||
                !mergeDepth(instruction.jumpTarget, depth)) return false;
            continue;
        case FloatExpressionOpcode::JumpIfFalse:
        case FloatExpressionOpcode::JumpIfTrue:
            if (depth < 1 || instruction.jumpTarget <= pc) return false;
            --depth;
            if (!mergeDepth(instruction.jumpTarget, depth)) return false;
            break;
        default:
            return false;
        }
        if (depth > kFloatExpressionMaxStack || !mergeDepth(pc + 1, depth))
            return false;
        if (depth > maximum) maximum = depth;
    }
    return depths[expression.instructionCount] == 1 &&
           maximum == expression.maxStackDepth;
}

inline FloatExpressionStatus evaluateProgramValue(
    const FloatAttributeExpression& expression, std::size_t elementIndex,
    float& output) noexcept {
    if (elementIndex >= expression.elementCount)
        return FloatExpressionStatus::InvalidInput;
    std::array<float, kFloatExpressionMaxStack> stack{};
    std::size_t depth = 0;
    std::size_t pc = 0;
    while (pc < expression.instructionCount) {
        const auto& instruction = expression.instructions[pc];
        switch (instruction.opcode) {
        case FloatExpressionOpcode::Constant:
            stack[depth++] = instruction.constant;
            break;
        case FloatExpressionOpcode::Attribute: {
            const float value = instruction.values[elementIndex];
            if (!std::isfinite(value)) return FloatExpressionStatus::InvalidInput;
            stack[depth++] = value;
            break;
        }
        case FloatExpressionOpcode::Negate:
            stack[depth - 1] = -stack[depth - 1];
            break;
        case FloatExpressionOpcode::Abs:
            stack[depth - 1] = std::fabs(stack[depth - 1]);
            break;
        case FloatExpressionOpcode::LogicalNot:
            stack[depth - 1] = stack[depth - 1] == 0.0f ? 1.0f : 0.0f;
            break;
        case FloatExpressionOpcode::ToBoolean:
            stack[depth - 1] = stack[depth - 1] == 0.0f ? 0.0f : 1.0f;
            break;
        case FloatExpressionOpcode::Jump:
            pc = instruction.jumpTarget;
            continue;
        case FloatExpressionOpcode::JumpIfFalse:
        case FloatExpressionOpcode::JumpIfTrue: {
            const bool truthy = stack[--depth] != 0.0f;
            if ((instruction.opcode == FloatExpressionOpcode::JumpIfFalse && !truthy) ||
                (instruction.opcode == FloatExpressionOpcode::JumpIfTrue && truthy))
                pc = instruction.jumpTarget;
            else
                ++pc;
            continue;
        }
        case FloatExpressionOpcode::Clamp: {
            const float value = stack[depth - 3];
            const float minimum = stack[depth - 2];
            const float maximum = stack[depth - 1];
            if (minimum > maximum) return FloatExpressionStatus::InvalidInput;
            stack[depth - 3] = value < minimum ? minimum
                : value > maximum ? maximum : value;
            depth -= 2;
            break;
        }
        case FloatExpressionOpcode::Add:
        case FloatExpressionOpcode::Subtract:
        case FloatExpressionOpcode::Multiply:
        case FloatExpressionOpcode::Divide:
        case FloatExpressionOpcode::Equal:
        case FloatExpressionOpcode::NotEqual:
        case FloatExpressionOpcode::Less:
        case FloatExpressionOpcode::LessOrEqual:
        case FloatExpressionOpcode::Greater:
        case FloatExpressionOpcode::GreaterOrEqual:
        case FloatExpressionOpcode::Minimum:
        case FloatExpressionOpcode::Maximum: {
            const float right = stack[depth - 1];
            const float left = stack[depth - 2];
            if (instruction.opcode == FloatExpressionOpcode::Divide && right == 0.0f)
                return FloatExpressionStatus::DivisionByZero;
            float value = 0.0f;
            if (instruction.opcode == FloatExpressionOpcode::Add) value = left + right;
            else if (instruction.opcode == FloatExpressionOpcode::Subtract) value = left - right;
            else if (instruction.opcode == FloatExpressionOpcode::Multiply) value = left * right;
            else if (instruction.opcode == FloatExpressionOpcode::Divide) value = left / right;
            else if (instruction.opcode == FloatExpressionOpcode::Equal) value = left == right ? 1.0f : 0.0f;
            else if (instruction.opcode == FloatExpressionOpcode::NotEqual) value = left != right ? 1.0f : 0.0f;
            else if (instruction.opcode == FloatExpressionOpcode::Less) value = left < right ? 1.0f : 0.0f;
            else if (instruction.opcode == FloatExpressionOpcode::LessOrEqual) value = left <= right ? 1.0f : 0.0f;
            else if (instruction.opcode == FloatExpressionOpcode::Greater) value = left > right ? 1.0f : 0.0f;
            else if (instruction.opcode == FloatExpressionOpcode::Minimum) value = left < right ? left : right;
            else if (instruction.opcode == FloatExpressionOpcode::Maximum) value = left > right ? left : right;
            else value = left >= right ? 1.0f : 0.0f;
            if (!std::isfinite(value)) return FloatExpressionStatus::InvalidInput;
            stack[depth - 2] = value;
            --depth;
            break;
        }
        }
        ++pc;
    }
    output = stack[0];
    return FloatExpressionStatus::Ready;
}
} // namespace expression_detail

inline FloatExpressionSourceCodecStatus makeFloatAttributeExpressionSource(
    std::uint64_t targetKey, AttributeOwner targetOwner, AttributeUnit targetUnit,
    std::string_view expression, FloatAttributeExpressionSource& destination) noexcept {
    if (expression.size() > kFloatExpressionMaxBytes)
        return FloatExpressionSourceCodecStatus::InvalidInput;
    FloatAttributeExpressionSource source;
    source.targetKey = targetKey;
    source.targetOwner = targetOwner;
    source.targetUnit = targetUnit;
    source.expressionLength = static_cast<std::uint16_t>(expression.size());
    for (std::size_t i = 0; i < expression.size(); ++i)
        source.expression[i] = expression[i];
    if (!expression_detail::validSourceRecord(source))
        return FloatExpressionSourceCodecStatus::InvalidInput;
    destination = source;
    return FloatExpressionSourceCodecStatus::Ready;
}

// MAEX version 1: a 20-byte header followed by exactly expressionLength ASCII
// bytes. Encode/decode use bounded local storage and preserve outputs on error.
inline FloatExpressionSourceCodecStatus encodeFloatAttributeExpressionSource(
    const FloatAttributeExpressionSource& source, std::span<std::byte> output) noexcept {
    if (!expression_detail::validSourceRecord(source))
        return FloatExpressionSourceCodecStatus::InvalidInput;
    const auto recordSize = kFloatExpressionSourceHeaderBytes + source.expressionLength;
    if (output.size() < recordSize) return FloatExpressionSourceCodecStatus::Defer;
    std::array<std::byte, kFloatExpressionSourceMaxRecordBytes> encoded{};
    const std::span<std::byte> bytes{encoded};
    expression_detail::writeSourceU32(bytes, 0, 0x5845414du);
    expression_detail::writeSourceU16(bytes, 4, 1);
    expression_detail::writeSourceU16(bytes, 6, static_cast<std::uint16_t>(recordSize));
    expression_detail::writeSourceU64(bytes, 8, source.targetKey);
    bytes[16] = static_cast<std::byte>(static_cast<std::uint8_t>(source.targetOwner));
    bytes[17] = static_cast<std::byte>(static_cast<std::uint8_t>(source.targetUnit));
    expression_detail::writeSourceU16(bytes, 18, source.expressionLength);
    for (std::size_t i = 0; i < source.expressionLength; ++i)
        bytes[kFloatExpressionSourceHeaderBytes + i] =
            static_cast<std::byte>(static_cast<unsigned char>(source.expression[i]));
    for (std::size_t i = 0; i < recordSize; ++i) output[i] = encoded[i];
    return FloatExpressionSourceCodecStatus::Ready;
}

inline FloatExpressionSourceCodecStatus decodeFloatAttributeExpressionSource(
    std::span<const std::byte> input,
    FloatAttributeExpressionSource& destination) noexcept {
    if (input.size() < kFloatExpressionSourceHeaderBytes ||
        input.size() > kFloatExpressionSourceMaxRecordBytes ||
        expression_detail::readSourceU32(input, 0) != 0x5845414du ||
        expression_detail::readSourceU16(input, 6) != input.size())
        return FloatExpressionSourceCodecStatus::InvalidInput;
    if (expression_detail::readSourceU16(input, 4) != 1)
        return FloatExpressionSourceCodecStatus::UnsupportedVersion;
    const auto expressionLength = expression_detail::readSourceU16(input, 18);
    if (expressionLength == 0 || expressionLength > kFloatExpressionMaxBytes ||
        kFloatExpressionSourceHeaderBytes + expressionLength != input.size())
        return FloatExpressionSourceCodecStatus::InvalidInput;
    FloatAttributeExpressionSource decoded;
    decoded.targetKey = expression_detail::readSourceU64(input, 8);
    decoded.targetOwner = static_cast<AttributeOwner>(
        std::to_integer<std::uint8_t>(input[16]));
    decoded.targetUnit = static_cast<AttributeUnit>(
        std::to_integer<std::uint8_t>(input[17]));
    decoded.expressionLength = expressionLength;
    for (std::size_t i = 0; i < expressionLength; ++i)
        decoded.expression[i] = static_cast<char>(
            std::to_integer<std::uint8_t>(input[kFloatExpressionSourceHeaderBytes + i]));
    if (!expression_detail::validSourceRecord(decoded))
        return FloatExpressionSourceCodecStatus::InvalidInput;
    destination = decoded;
    return FloatExpressionSourceCodecStatus::Ready;
}

// Compile a bounded arithmetic expression. Attribute references use @name;
// names and float spans are borrowed, and the compiled program retains only
// value spans, not source text or schema strings.
inline FloatExpressionStatus compileFloatAttributeExpression(
    std::string_view source,
    std::span<const FloatAttributeExpressionBinding> bindings,
    std::size_t elementCount, AttributeUnit targetUnit,
    FloatAttributeExpression& destination) noexcept {
    if (source.empty()) return FloatExpressionStatus::InvalidExpression;
    if (source.size() > kFloatExpressionMaxBytes ||
        bindings.size() > kFloatExpressionMaxBindings)
        return FloatExpressionStatus::CapacityExceeded;
    if (!expression_detail::validUnit(targetUnit))
        return FloatExpressionStatus::InvalidInput;
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto& binding = bindings[i];
        if (binding.name.empty() || binding.name.size() > kFloatExpressionMaxNameBytes ||
            !expression_detail::nameStart(binding.name.front()) ||
            !expression_detail::validUnit(binding.unit) ||
            binding.values.size() != elementCount)
            return FloatExpressionStatus::InvalidInput;
        for (std::size_t c = 1; c < binding.name.size(); ++c)
            if (!expression_detail::nameContinue(binding.name[c]))
                return FloatExpressionStatus::InvalidInput;
        for (std::size_t j = 0; j < i; ++j)
            if (bindings[j].name == binding.name)
                return FloatExpressionStatus::InvalidInput;
    }
    FloatAttributeExpression compiled;
    compiled.elementCount = elementCount;
    expression_detail::Parser parser{source, bindings, elementCount, compiled};
    expression_detail::TypeInfo result;
    const auto status = parser.parse(result);
    if (status != FloatExpressionStatus::Ready) return status;
    if (!result.literalOnly && result.unit != targetUnit)
        return FloatExpressionStatus::TypeOrUnitMismatch;
    compiled.resultUnit = targetUnit;
    if (!expression_detail::validProgram(compiled))
        return FloatExpressionStatus::InvalidExpression;
    destination = compiled;
    return FloatExpressionStatus::Ready;
}

inline FloatExpressionStatus compileFloatAttributeExpression(
    const FloatAttributeExpressionSource& source,
    std::span<const FloatAttributeExpressionBinding> bindings,
    std::size_t elementCount,
    FloatAttributeExpression& destination) noexcept {
    if (!expression_detail::validSourceRecord(source))
        return FloatExpressionStatus::InvalidInput;
    FloatAttributeExpression compiled;
    const auto status = compileFloatAttributeExpression(source.text(), bindings,
        elementCount, source.targetUnit, compiled);
    if (status != FloatExpressionStatus::Ready) return status;
    compiled.targetKey = source.targetKey;
    compiled.targetOwner = source.targetOwner;
    destination = compiled;
    return FloatExpressionStatus::Ready;
}

inline FloatExpressionStatus evaluateFloatAttributeExpression(
    const FloatAttributeExpression& expression, std::size_t elementIndex,
    float& output) noexcept {
    if (!expression_detail::validProgram(expression))
        return FloatExpressionStatus::InvalidInput;
    return expression_detail::evaluateProgramValue(expression, elementIndex, output);
}

struct FloatAttributeAuthoringBatchCommand {
    FloatAttributeEditPlan plan;
    const FloatAttributeExpression* expression = nullptr;
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit targetUnit = AttributeUnit::Dimensionless;
    std::span<const std::size_t> candidates;
};

// Evaluate plan and expression commands from one pre-commit snapshot and apply
// all target columns only after every command succeeds. Repeated targets need
// disjoint, ascending candidate ranges. Expression bindings remain unchanged
// through evaluation and read the same pre-commit snapshot as plan commands.
// Mutable target spans are pairwise non-overlapping and single-writer owned;
// command, candidate, and workspace storage must not alias target/input data.
inline FloatAttributeHistoryStatus evaluateFloatAttributeAuthoringBatch(
    std::uint64_t targetInstanceKey,
    std::span<const FloatAttributeAuthoringBatchCommand> commands,
    std::span<const AttributeColumn> columns,
    std::span<const FloatAttributeMutableColumn> mutableTargets,
    std::span<float> resultScratch,
    std::span<FloatAttributeEditChange> journal,
    std::span<FloatAttributeEditBatchTargetReceipt> receiptTargets,
    FloatAttributeEditBatchReceipt& receipt) noexcept {
    if (targetInstanceKey == 0 || commands.empty())
        return FloatAttributeHistoryStatus::InvalidInput;
    std::size_t totalChanges = 0;
    std::size_t targetGroupCount = 0;
    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex) {
        const auto& command = commands[commandIndex];
        const bool expressionCommand = command.expression != nullptr;
        const auto targetKey = expressionCommand ? command.targetKey : command.plan.targetKey;
        const auto targetOwner = expressionCommand ? command.targetOwner : command.plan.targetOwner;
        const auto targetUnit = expressionCommand ? command.targetUnit : command.plan.targetUnit;
        if (expressionCommand) {
            const auto& expression = *command.expression;
            if (!expression_detail::validProgram(expression) || targetKey == 0 ||
                !expression_detail::validOwner(targetOwner) ||
                !expression_detail::validUnit(targetUnit) ||
                expression.targetOwner != targetOwner ||
                targetUnit != expression.resultUnit ||
                (expression.targetKey != 0 && expression.targetKey != targetKey))
                return FloatAttributeHistoryStatus::InvalidInput;
        } else if (!detail::validFloatAttributeEditPlan(command.plan)) {
            return FloatAttributeHistoryStatus::InvalidInput;
        }
        if (command.candidates.empty())
            return FloatAttributeHistoryStatus::InvalidInput;
        if (commandIndex != 0) {
            const auto& previousCommand = commands[commandIndex - 1];
            const bool previousExpression = previousCommand.expression != nullptr;
            const auto previousKey = previousExpression
                ? previousCommand.targetKey : previousCommand.plan.targetKey;
            const auto previousOwner = previousExpression
                ? previousCommand.targetOwner : previousCommand.plan.targetOwner;
            if (previousKey == targetKey && previousOwner == targetOwner) {
                if (previousCommand.candidates.empty() ||
                    previousCommand.candidates.back() >= command.candidates.front())
                    return FloatAttributeHistoryStatus::InvalidInput;
            } else {
                if (!detail::attributeTargetLess(previousOwner, previousKey,
                        targetOwner, targetKey))
                    return FloatAttributeHistoryStatus::InvalidInput;
                ++targetGroupCount;
            }
        } else {
            ++targetGroupCount;
        }

        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets) {
            if (candidate.key != targetKey || candidate.owner != targetOwner) continue;
            if (target) return FloatAttributeHistoryStatus::InvalidInput;
            target = &candidate;
        }
        if (!target || target->unit != targetUnit ||
            !detail::validAttributeOwner(target->owner) ||
            !detail::validAttributeUnit(target->unit))
            return FloatAttributeHistoryStatus::InvalidInput;

        const AttributeColumn* descriptor = nullptr;
        const std::span<const float>* readOnlyValues = nullptr;
        if (detail::resolveFloatAttributeColumn(columns, targetKey, targetOwner,
                targetUnit, descriptor, readOnlyValues) != AttributeTransferStatus::Ready ||
            target->values.size() != readOnlyValues->size() ||
            target->values.data() != readOnlyValues->data() ||
            (expressionCommand &&
             target->values.size() != command.expression->elementCount))
            return FloatAttributeHistoryStatus::InvalidInput;
        for (std::size_t i = 0; i < command.candidates.size(); ++i)
            if (command.candidates[i] >= target->values.size() ||
                (i != 0 && command.candidates[i - 1] >= command.candidates[i]))
                return FloatAttributeHistoryStatus::InvalidInput;
        if (command.candidates.size() >
            std::numeric_limits<std::size_t>::max() - totalChanges)
            return FloatAttributeHistoryStatus::InvalidInput;
        totalChanges += command.candidates.size();
    }
    if (receiptTargets.size() < targetGroupCount ||
        resultScratch.size() < totalChanges || journal.size() < totalChanges)
        return FloatAttributeHistoryStatus::Defer;

    std::size_t offset = 0;
    for (const auto& command : commands) {
        const bool expressionCommand = command.expression != nullptr;
        const auto targetKey = expressionCommand ? command.targetKey : command.plan.targetKey;
        const auto targetOwner = expressionCommand ? command.targetOwner : command.plan.targetOwner;
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets)
            if (candidate.key == targetKey && candidate.owner == targetOwner) {
                target = &candidate;
                break;
            }
        for (std::size_t i = 0; i < command.candidates.size(); ++i) {
            const auto index = command.candidates[i];
            if (!std::isfinite(target->values[index]))
                return FloatAttributeHistoryStatus::InvalidInput;
            journal[offset + i] = {index, target->values[index], 0.0f};
        }

        const auto scratch = resultScratch.subspan(offset, command.candidates.size());
        if (expressionCommand) {
            for (std::size_t i = 0; i < command.candidates.size(); ++i) {
                const auto status = expression_detail::evaluateProgramValue(
                    *command.expression, command.candidates[i], scratch[i]);
                if (status != FloatExpressionStatus::Ready)
                    return status == FloatExpressionStatus::Defer
                        ? FloatAttributeHistoryStatus::Defer
                        : FloatAttributeHistoryStatus::InvalidInput;
            }
        } else {
            const auto status = evaluateFloatAttributeEditResults(
                command.plan, columns, target->values, command.candidates, scratch);
            if (status != AttributeTransferStatus::Ready)
                return status == AttributeTransferStatus::Defer
                    ? FloatAttributeHistoryStatus::Defer
                    : FloatAttributeHistoryStatus::InvalidInput;
        }
        offset += command.candidates.size();
    }

    offset = 0;
    std::size_t targetGroupIndex = 0;
    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex) {
        const auto& command = commands[commandIndex];
        const bool expressionCommand = command.expression != nullptr;
        const auto targetKey = expressionCommand ? command.targetKey : command.plan.targetKey;
        const auto targetOwner = expressionCommand ? command.targetOwner : command.plan.targetOwner;
        const auto targetUnit = expressionCommand ? command.targetUnit : command.plan.targetUnit;
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets)
            if (candidate.key == targetKey && candidate.owner == targetOwner) {
                target = &candidate;
                break;
            }
        for (std::size_t i = 0; i < command.candidates.size(); ++i) {
            const auto index = command.candidates[i];
            journal[offset + i].after = resultScratch[offset + i];
            target->values[index] = resultScratch[offset + i];
        }
        const auto& previousCommand = commandIndex == 0
            ? command : commands[commandIndex - 1];
        const bool previousExpression = previousCommand.expression != nullptr;
        const auto previousKey = previousExpression
            ? previousCommand.targetKey : previousCommand.plan.targetKey;
        const auto previousOwner = previousExpression
            ? previousCommand.targetOwner : previousCommand.plan.targetOwner;
        const bool startsTargetGroup = commandIndex == 0 ||
            previousKey != targetKey || previousOwner != targetOwner;
        if (startsTargetGroup) {
            receiptTargets[targetGroupIndex] = {targetKey, targetOwner, targetUnit,
                offset, 0};
            ++targetGroupIndex;
        }
        receiptTargets[targetGroupIndex - 1].changeCount += command.candidates.size();
        offset += command.candidates.size();
    }
    receipt = {targetInstanceKey, targetGroupCount, totalChanges};
    return FloatAttributeHistoryStatus::Applied;
}

// Evaluate the expression for every candidate first, then assign the target
// column as one non-allocating operation. Candidate indices must be sorted and
// unique. Scratch must not overlap target, candidates, or bound input columns.
inline FloatExpressionStatus assignFloatAttributeExpression(
    const FloatAttributeExpression& expression,
    std::uint64_t targetKey, AttributeOwner targetOwner,
    AttributeUnit targetUnit,
    std::span<float> targetValues,
    std::span<const std::size_t> candidates,
    std::span<float> scratch) noexcept {
    if (!expression_detail::validProgram(expression) ||
        targetKey == 0 || !expression_detail::validOwner(targetOwner) ||
        (expression.targetKey != 0 &&
         (expression.targetKey != targetKey || expression.targetOwner != targetOwner)) ||
        !expression_detail::validUnit(targetUnit) || targetUnit != expression.resultUnit ||
        targetValues.size() != expression.elementCount)
        return FloatExpressionStatus::InvalidInput;
    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i] >= targetValues.size() ||
            (i != 0 && candidates[i - 1] >= candidates[i]))
            return FloatExpressionStatus::InvalidInput;
    if (scratch.size() < candidates.size()) return FloatExpressionStatus::Defer;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto status = expression_detail::evaluateProgramValue(
            expression, candidates[i], scratch[i]);
        if (status != FloatExpressionStatus::Ready) return status;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i)
        targetValues[candidates[i]] = scratch[i];
    return FloatExpressionStatus::Ready;
}

// Apply an expression and capture a caller-owned bounded Undo receipt. All
// evaluation completes before the receipt and target are committed. Journal
// and scratch spans must not overlap the target, candidates, or input columns.
inline FloatAttributeHistoryStatus assignFloatAttributeExpressionWithReceipt(
    const FloatAttributeExpression& expression,
    std::uint64_t targetInstanceKey,
    std::uint64_t targetKey, AttributeOwner targetOwner,
    AttributeUnit targetUnit,
    std::span<float> targetValues,
    std::span<const std::size_t> candidates,
    std::span<float> resultScratch,
    std::span<FloatAttributeEditChange> journal,
    FloatAttributeEditReceipt& receipt) noexcept {
    if (!expression_detail::validProgram(expression) || targetInstanceKey == 0 || targetKey == 0 ||
        !expression_detail::validOwner(targetOwner) ||
        (expression.targetKey != 0 &&
         (expression.targetKey != targetKey || expression.targetOwner != targetOwner)) ||
        !expression_detail::validUnit(targetUnit) || targetUnit != expression.resultUnit ||
        targetValues.size() != expression.elementCount)
        return FloatAttributeHistoryStatus::InvalidInput;
    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i] >= targetValues.size() ||
            (i != 0 && candidates[i - 1] >= candidates[i]))
            return FloatAttributeHistoryStatus::InvalidInput;
    if (resultScratch.size() < candidates.size() || journal.size() < candidates.size())
        return FloatAttributeHistoryStatus::Defer;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto index = candidates[i];
        if (!std::isfinite(targetValues[index]) ||
            expression_detail::evaluateProgramValue(expression, index, resultScratch[i]) !=
                FloatExpressionStatus::Ready)
            return FloatAttributeHistoryStatus::InvalidInput;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto index = candidates[i];
        journal[i] = {index, targetValues[index], resultScratch[i]};
    }
    for (std::size_t i = 0; i < candidates.size(); ++i)
        targetValues[candidates[i]] = resultScratch[i];
    receipt = {targetInstanceKey, targetKey, targetOwner, targetUnit, candidates.size()};
    return FloatAttributeHistoryStatus::Applied;
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
