#pragma once

#include "MaterialTransfer.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <variant>

namespace ArtifactCore::Experimental::MaterialTransfer {

// Borrowed read-only columns; the caller freezes schema/storage for evaluation.
// Numeric keys are resolved outside simulation steps. No per-element dictionary.
enum class AttributeOwner : std::uint8_t { Particle, Fragment, Vertex, Face, Layer };
enum class AttributeUnit : std::uint8_t { Dimensionless, Seconds, Kilograms, Meters };
using AttributeValues = std::variant<std::span<const float>,
                                     std::span<const std::uint64_t>,
                                     std::span<const Vector3>>;

struct AttributeColumn {
    std::uint64_t key = 0;
    AttributeOwner owner = AttributeOwner::Particle;
    AttributeUnit unit = AttributeUnit::Dimensionless;
    AttributeValues values;
};

// Missing columns return null: adapters must choose an explicit default or reject.
// No interpolation or unit conversion is implied by this view.
// Call valid() on the cold path before lookup; freeze columns during evaluation.
class AttributeView {
public:
    constexpr AttributeView(std::span<const AttributeColumn> columns,
                            std::size_t elementCount) noexcept
        : columns_(columns), elementCount_(elementCount) {}

    bool valid() const noexcept {
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            const auto& column = columns_[i];
            if (column.key == 0 ||
                static_cast<unsigned>(column.owner) > static_cast<unsigned>(AttributeOwner::Layer) ||
                static_cast<unsigned>(column.unit) > static_cast<unsigned>(AttributeUnit::Meters) ||
                std::visit([](auto values) { return values.size(); }, column.values) != elementCount_) {
                return false;
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (columns_[j].key == column.key && columns_[j].owner == column.owner)
                    return false;
            }
        }
        return true;
    }

    template<class T>
    const std::span<const T>* find(std::uint64_t key, AttributeOwner owner,
                                   AttributeUnit unit) const noexcept {
        for (const auto& column : columns_) {
            if (column.key == key && column.owner == owner && column.unit == unit)
                return std::get_if<std::span<const T>>(&column.values);
        }
        return nullptr;
    }

private:
    std::span<const AttributeColumn> columns_;
    std::size_t elementCount_;
};

enum class MissingAttribute : std::uint8_t { Reject, UseDefault };
enum class AttributeTransferStatus : std::uint8_t {
    Ready, Missing, TypeOrUnitMismatch, InvalidInput, Defer, AlreadyExists
};

enum class FloatAttributeOperation : std::uint8_t {
    Set, Add, Subtract, Multiply, Divide, Minimum, Maximum
};

enum class FloatAttributeComparison : std::uint8_t {
    Equal, NotEqual, Less, LessOrEqual, Greater, GreaterOrEqual
};

enum class FloatAttributeHistoryStatus : std::uint8_t {
    Applied, Undone, Redone, InvalidInput, Defer, Conflict
};

struct FloatAttributeEditChange {
    std::size_t elementIndex = 0;
    float before = 0.0f;
    float after = 0.0f;
};

struct FloatAttributeEditReceipt {
    std::uint64_t targetInstanceKey = 0;
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit targetUnit = AttributeUnit::Dimensionless;
    std::size_t changeCount = 0;
};

struct FloatAttributeMutableColumn {
    std::uint64_t key = 0;
    AttributeOwner owner = AttributeOwner::Particle;
    AttributeUnit unit = AttributeUnit::Dimensionless;
    std::span<float> values;
};

struct FloatAttributeEditBatchTargetReceipt {
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit targetUnit = AttributeUnit::Dimensionless;
    std::size_t changeOffset = 0;
    std::size_t changeCount = 0;
};

struct FloatAttributeEditBatchReceipt {
    std::uint64_t targetInstanceKey = 0;
    std::size_t targetCount = 0;
    std::size_t changeCount = 0;
};

enum class FloatAttributeHistoryDirection : std::uint8_t { Undo, Redo };

// A compact, parser-independent authoring instruction. A zero source key means
// use operand; a zero condition key means compare against conditionOperand.
// Column keys are resolved before evaluation and are stable schema identifiers.
struct FloatAttributeEditPlan {
    std::uint64_t targetKey = 0;
    AttributeOwner targetOwner = AttributeOwner::Particle;
    AttributeUnit targetUnit = AttributeUnit::Dimensionless;
    std::uint64_t sourceKey = 0;
    FloatAttributeOperation operation = FloatAttributeOperation::Set;
    float operand = 0.0f;
    bool hasCondition = false;
    std::uint64_t conditionKey = 0;
    AttributeOwner conditionOwner = AttributeOwner::Particle;
    AttributeUnit conditionUnit = AttributeUnit::Dimensionless;
    FloatAttributeComparison comparison = FloatAttributeComparison::Greater;
    float conditionOperand = 0.0f;
};

struct FloatAttributeEditBatchCommand {
    FloatAttributeEditPlan plan;
    std::span<const std::size_t> candidates;
};

namespace detail {
inline bool validAttributeOwner(AttributeOwner owner) noexcept {
    return static_cast<unsigned>(owner) <= static_cast<unsigned>(AttributeOwner::Layer);
}
inline bool validAttributeUnit(AttributeUnit unit) noexcept {
    return static_cast<unsigned>(unit) <= static_cast<unsigned>(AttributeUnit::Meters);
}
inline bool attributeTargetLess(AttributeOwner leftOwner, std::uint64_t leftKey,
                                AttributeOwner rightOwner, std::uint64_t rightKey) noexcept {
    if (leftOwner != rightOwner)
        return static_cast<unsigned>(leftOwner) < static_cast<unsigned>(rightOwner);
    return leftKey < rightKey;
}
inline bool validFloatAttributeEditPlan(const FloatAttributeEditPlan& plan) noexcept {
    if (plan.targetKey == 0 ||
        static_cast<unsigned>(plan.targetOwner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(plan.targetUnit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        static_cast<unsigned>(plan.operation) > static_cast<unsigned>(FloatAttributeOperation::Maximum) ||
        static_cast<unsigned>(plan.conditionOwner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(plan.conditionUnit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        static_cast<unsigned>(plan.comparison) > static_cast<unsigned>(FloatAttributeComparison::GreaterOrEqual) ||
        !std::isfinite(plan.operand) || !std::isfinite(plan.conditionOperand) ||
        (!plan.hasCondition && plan.conditionKey != 0) ||
        (plan.sourceKey != 0 &&
         (plan.operation == FloatAttributeOperation::Multiply ||
          plan.operation == FloatAttributeOperation::Divide)) ||
        (plan.sourceKey == 0 && plan.operation == FloatAttributeOperation::Divide &&
         plan.operand == 0.0f))
        return false;
    return true;
}

inline AttributeTransferStatus resolveFloatAttributeColumn(
    std::span<const AttributeColumn> columns, std::uint64_t key,
    AttributeOwner owner, AttributeUnit unit,
    const AttributeColumn*& descriptor,
    const std::span<const float>*& values) noexcept {
    descriptor = nullptr;
    values = nullptr;
    for (const auto& column : columns) {
        if (column.key != key || column.owner != owner) continue;
        if (descriptor) return AttributeTransferStatus::InvalidInput;
        descriptor = &column;
    }
    if (!descriptor) return AttributeTransferStatus::Missing;
    if (descriptor->unit != unit) return AttributeTransferStatus::TypeOrUnitMismatch;
    values = std::get_if<std::span<const float>>(&descriptor->values);
    return values ? AttributeTransferStatus::Ready
                  : AttributeTransferStatus::TypeOrUnitMismatch;
}

inline bool compareFloatAttribute(float value, FloatAttributeComparison comparison,
                                  float operand) noexcept {
    switch (comparison) {
    case FloatAttributeComparison::Equal: return value == operand;
    case FloatAttributeComparison::NotEqual: return value != operand;
    case FloatAttributeComparison::Less: return value < operand;
    case FloatAttributeComparison::LessOrEqual: return value <= operand;
    case FloatAttributeComparison::Greater: return value > operand;
    case FloatAttributeComparison::GreaterOrEqual: return value >= operand;
    }
    return false;
}

inline bool applyFloatAttributeOperation(float current, float operand,
                                        FloatAttributeOperation operation,
                                        float& result) noexcept {
    switch (operation) {
    case FloatAttributeOperation::Set: result = operand; break;
    case FloatAttributeOperation::Add: result = current + operand; break;
    case FloatAttributeOperation::Subtract: result = current - operand; break;
    case FloatAttributeOperation::Multiply: result = current * operand; break;
    case FloatAttributeOperation::Divide:
        if (operand == 0.0f) return false;
        result = current / operand;
        break;
    case FloatAttributeOperation::Minimum:
        result = current < operand ? current : operand;
        break;
    case FloatAttributeOperation::Maximum:
        result = current > operand ? current : operand;
        break;
    }
    return std::isfinite(result);
}
} // namespace detail

inline constexpr std::size_t kFloatAttributeEditPlanEncodedSize = 47;
enum class AttributePlanCodecStatus : std::uint8_t {
    Ready, InvalidInput, Defer, UnsupportedVersion
};

namespace detail {
inline void writePlanU16(std::span<std::byte> bytes, std::size_t offset,
                         std::uint16_t value) noexcept {
    for (std::size_t i = 0; i < 2; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline void writePlanU32(std::span<std::byte> bytes, std::size_t offset,
                         std::uint32_t value) noexcept {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline void writePlanU64(std::span<std::byte> bytes, std::size_t offset,
                         std::uint64_t value) noexcept {
    for (std::size_t i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xffu);
}
inline std::uint16_t readPlanU16(std::span<const std::byte> bytes,
                                 std::size_t offset) noexcept {
    std::uint16_t value = 0;
    for (std::size_t i = 0; i < 2; ++i)
        value |= static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}
inline std::uint32_t readPlanU32(std::span<const std::byte> bytes,
                                 std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}
inline std::uint64_t readPlanU64(std::span<const std::byte> bytes,
                                 std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}
inline void writePlanFloat(std::span<std::byte> bytes, std::size_t offset,
                           float value) noexcept {
    writePlanU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}
inline float readPlanFloat(std::span<const std::byte> bytes,
                           std::size_t offset) noexcept {
    return std::bit_cast<float>(readPlanU32(bytes, offset));
}
} // namespace detail

// Version 1 is a fixed 47-byte little-endian record ("MATP", version, size,
// followed by the plan fields). No allocation or partial write on failure.
inline AttributePlanCodecStatus encodeFloatAttributeEditPlan(
    const FloatAttributeEditPlan& plan, std::span<std::byte> output) noexcept {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    static_assert(std::numeric_limits<float>::is_iec559);
    if (!detail::validFloatAttributeEditPlan(plan))
        return AttributePlanCodecStatus::InvalidInput;
    if (output.size() < kFloatAttributeEditPlanEncodedSize)
        return AttributePlanCodecStatus::Defer;
    std::array<std::byte, kFloatAttributeEditPlanEncodedSize> encoded{};
    const std::span<std::byte> bytes{encoded};
    detail::writePlanU32(bytes, 0, 0x5054414du);
    detail::writePlanU16(bytes, 4, 1);
    detail::writePlanU16(bytes, 6, static_cast<std::uint16_t>(kFloatAttributeEditPlanEncodedSize));
    detail::writePlanU64(bytes, 8, plan.targetKey);
    bytes[16] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.targetOwner));
    bytes[17] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.targetUnit));
    detail::writePlanU64(bytes, 18, plan.sourceKey);
    bytes[26] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.operation));
    detail::writePlanFloat(bytes, 27, plan.operand);
    bytes[31] = static_cast<std::byte>(plan.hasCondition ? 1 : 0);
    detail::writePlanU64(bytes, 32, plan.conditionKey);
    bytes[40] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.conditionOwner));
    bytes[41] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.conditionUnit));
    bytes[42] = static_cast<std::byte>(static_cast<std::uint8_t>(plan.comparison));
    detail::writePlanFloat(bytes, 43, plan.conditionOperand);
    for (std::size_t i = 0; i < encoded.size(); ++i) output[i] = encoded[i];
    return AttributePlanCodecStatus::Ready;
}

// Decode into a temporary first; destination remains unchanged for malformed
// input and versions this reader does not understand.
inline AttributePlanCodecStatus decodeFloatAttributeEditPlan(
    std::span<const std::byte> input, FloatAttributeEditPlan& destination) noexcept {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    static_assert(std::numeric_limits<float>::is_iec559);
    if (input.size() != kFloatAttributeEditPlanEncodedSize ||
        detail::readPlanU32(input, 0) != 0x5054414du ||
        detail::readPlanU16(input, 6) != kFloatAttributeEditPlanEncodedSize)
        return AttributePlanCodecStatus::InvalidInput;
    if (detail::readPlanU16(input, 4) != 1)
        return AttributePlanCodecStatus::UnsupportedVersion;
    const auto owner = std::to_integer<std::uint8_t>(input[16]);
    const auto unit = std::to_integer<std::uint8_t>(input[17]);
    const auto operation = std::to_integer<std::uint8_t>(input[26]);
    const auto hasCondition = std::to_integer<std::uint8_t>(input[31]);
    const auto conditionOwner = std::to_integer<std::uint8_t>(input[40]);
    const auto conditionUnit = std::to_integer<std::uint8_t>(input[41]);
    const auto comparison = std::to_integer<std::uint8_t>(input[42]);
    if (hasCondition > 1) return AttributePlanCodecStatus::InvalidInput;
    FloatAttributeEditPlan decoded;
    decoded.targetKey = detail::readPlanU64(input, 8);
    decoded.targetOwner = static_cast<AttributeOwner>(owner);
    decoded.targetUnit = static_cast<AttributeUnit>(unit);
    decoded.sourceKey = detail::readPlanU64(input, 18);
    decoded.operation = static_cast<FloatAttributeOperation>(operation);
    decoded.operand = detail::readPlanFloat(input, 27);
    decoded.hasCondition = hasCondition != 0;
    decoded.conditionKey = detail::readPlanU64(input, 32);
    decoded.conditionOwner = static_cast<AttributeOwner>(conditionOwner);
    decoded.conditionUnit = static_cast<AttributeUnit>(conditionUnit);
    decoded.comparison = static_cast<FloatAttributeComparison>(comparison);
    decoded.conditionOperand = detail::readPlanFloat(input, 43);
    if (!detail::validFloatAttributeEditPlan(decoded))
        return AttributePlanCodecStatus::InvalidInput;
    destination = decoded;
    return AttributePlanCodecStatus::Ready;
}

// Evaluate one compact edit instruction into scratch without changing target.
// All referenced attributes must be float columns. Source columns must match
// target owner/unit; column-to-column multiply/divide has no defined result unit.
// Scratch must not overlap candidate indices, target, or any input column.
inline AttributeTransferStatus evaluateFloatAttributeEditResults(
    const FloatAttributeEditPlan& plan,
    std::span<const AttributeColumn> columns,
    std::span<float> targetValues,
    std::span<const std::size_t> candidates,
    std::span<float> scratch) noexcept {
    if (!detail::validFloatAttributeEditPlan(plan))
        return AttributeTransferStatus::InvalidInput;

    const AttributeColumn* targetDescriptor = nullptr;
    const std::span<const float>* targetReadOnly = nullptr;
    auto status = detail::resolveFloatAttributeColumn(columns, plan.targetKey,
        plan.targetOwner, plan.targetUnit, targetDescriptor, targetReadOnly);
    if (status != AttributeTransferStatus::Ready) return status;
    if (targetValues.size() != targetReadOnly->size() ||
        targetValues.data() != targetReadOnly->data())
        return AttributeTransferStatus::InvalidInput;

    const AttributeColumn* sourceDescriptor = nullptr;
    const std::span<const float>* sourceValues = nullptr;
    if (plan.sourceKey != 0) {
        status = detail::resolveFloatAttributeColumn(columns, plan.sourceKey,
            plan.targetOwner, plan.targetUnit, sourceDescriptor, sourceValues);
        if (status != AttributeTransferStatus::Ready) return status;
        if (sourceValues->size() != targetValues.size()) return AttributeTransferStatus::InvalidInput;
        if (plan.operation == FloatAttributeOperation::Multiply ||
            plan.operation == FloatAttributeOperation::Divide)
            return AttributeTransferStatus::InvalidInput;
    } else if (plan.operation == FloatAttributeOperation::Divide && plan.operand == 0.0f) {
        return AttributeTransferStatus::InvalidInput;
    }

    const AttributeColumn* conditionDescriptor = nullptr;
    const std::span<const float>* conditionValues = nullptr;
    if (plan.hasCondition && plan.conditionKey != 0) {
        status = detail::resolveFloatAttributeColumn(columns, plan.conditionKey,
            plan.conditionOwner, plan.conditionUnit, conditionDescriptor, conditionValues);
        if (status != AttributeTransferStatus::Ready) return status;
        if (conditionValues->size() != targetValues.size()) return AttributeTransferStatus::InvalidInput;
    }

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i] >= targetValues.size() ||
            (i != 0 && candidates[i - 1] >= candidates[i]))
            return AttributeTransferStatus::InvalidInput;
    }
    if (scratch.size() < candidates.size()) return AttributeTransferStatus::Defer;

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto index = candidates[i];
        const float current = targetValues[index];
        const float operand = sourceValues ? (*sourceValues)[index] : plan.operand;
        if (!std::isfinite(current) || !std::isfinite(operand))
            return AttributeTransferStatus::InvalidInput;
        float result = current;
        if (!plan.hasCondition || detail::compareFloatAttribute(
                conditionValues ? (*conditionValues)[index] : plan.conditionOperand,
                plan.comparison, plan.conditionOperand)) {
            const float conditionValue = conditionValues ? (*conditionValues)[index]
                                                         : plan.conditionOperand;
            if (!std::isfinite(conditionValue)) return AttributeTransferStatus::InvalidInput;
            if (!detail::applyFloatAttributeOperation(current, operand,
                    plan.operation, result))
                return AttributeTransferStatus::InvalidInput;
        }
        if (plan.hasCondition && conditionValues &&
            !std::isfinite((*conditionValues)[index]))
            return AttributeTransferStatus::InvalidInput;
        scratch[i] = result;
    }
    return AttributeTransferStatus::Ready;
}

// Evaluate and commit one edit. All results are validated before the target
// column is changed.
inline AttributeTransferStatus evaluateFloatAttributeEdit(
    const FloatAttributeEditPlan& plan,
    std::span<const AttributeColumn> columns,
    std::span<float> targetValues,
    std::span<const std::size_t> candidates,
    std::span<float> scratch) noexcept {
    const auto status = evaluateFloatAttributeEditResults(
        plan, columns, targetValues, candidates, scratch);
    if (status != AttributeTransferStatus::Ready) return status;
    for (std::size_t i = 0; i < candidates.size(); ++i)
        targetValues[candidates[i]] = scratch[i];
    return AttributeTransferStatus::Ready;
}

// Apply plan edits as one bounded snapshot transaction. Every command reads
// the same pre-commit columns. Commands for one target must select disjoint,
// ascending index ranges and be adjacent in target-key order.
// Mutable target spans must be pairwise non-overlapping, with one caller-owned
// writer and no observers during commit. Workspace, command, and candidate
// storage must be mutually non-overlapping and separate from target/input data.
inline FloatAttributeHistoryStatus evaluateFloatAttributeEditBatch(
    std::uint64_t targetInstanceKey,
    std::span<const FloatAttributeEditBatchCommand> commands,
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
        const auto& plan = command.plan;
        if (!detail::validFloatAttributeEditPlan(plan))
            return FloatAttributeHistoryStatus::InvalidInput;
        if (command.candidates.empty())
            return FloatAttributeHistoryStatus::InvalidInput;
        if (commandIndex != 0) {
            const auto& previous = commands[commandIndex - 1];
            const auto& previousPlan = previous.plan;
            if (previousPlan.targetKey == plan.targetKey &&
                previousPlan.targetOwner == plan.targetOwner) {
                if (previous.candidates.empty() ||
                    previous.candidates.back() >= command.candidates.front())
                    return FloatAttributeHistoryStatus::InvalidInput;
            } else {
                if (!detail::attributeTargetLess(previousPlan.targetOwner,
                        previousPlan.targetKey, plan.targetOwner, plan.targetKey))
                    return FloatAttributeHistoryStatus::InvalidInput;
                ++targetGroupCount;
            }
        } else {
            ++targetGroupCount;
        }
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets) {
            if (candidate.key != plan.targetKey || candidate.owner != plan.targetOwner)
                continue;
            if (target) return FloatAttributeHistoryStatus::InvalidInput;
            target = &candidate;
        }
        if (!target || target->unit != plan.targetUnit ||
            !detail::validAttributeOwner(target->owner) ||
            !detail::validAttributeUnit(target->unit))
            return FloatAttributeHistoryStatus::InvalidInput;

        const AttributeColumn* targetDescriptor = nullptr;
        const std::span<const float>* targetReadOnly = nullptr;
        const auto targetStatus = detail::resolveFloatAttributeColumn(
            columns, plan.targetKey, plan.targetOwner, plan.targetUnit,
            targetDescriptor, targetReadOnly);
        if (targetStatus != AttributeTransferStatus::Ready ||
            target->values.size() != targetReadOnly->size() ||
            target->values.data() != targetReadOnly->data())
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
        const auto& plan = command.plan;
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets)
            if (candidate.key == plan.targetKey && candidate.owner == plan.targetOwner) {
                target = &candidate;
                break;
            }
        for (std::size_t i = 0; i < command.candidates.size(); ++i) {
            const auto index = command.candidates[i];
            if (!std::isfinite(target->values[index]))
                return FloatAttributeHistoryStatus::InvalidInput;
            journal[offset + i] = {index, target->values[index], 0.0f};
        }
        const auto status = evaluateFloatAttributeEditResults(
            plan, columns, target->values, command.candidates,
            resultScratch.subspan(offset, command.candidates.size()));
        if (status != AttributeTransferStatus::Ready)
            return status == AttributeTransferStatus::Defer
                ? FloatAttributeHistoryStatus::Defer
                : FloatAttributeHistoryStatus::InvalidInput;
        offset += command.candidates.size();
    }

    offset = 0;
    std::size_t targetGroupIndex = 0;
    for (std::size_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex) {
        const auto& command = commands[commandIndex];
        const auto& plan = command.plan;
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets)
            if (candidate.key == plan.targetKey && candidate.owner == plan.targetOwner) {
                target = &candidate;
                break;
            }
        for (std::size_t i = 0; i < command.candidates.size(); ++i) {
            const auto index = command.candidates[i];
            journal[offset + i].after = resultScratch[offset + i];
            target->values[index] = resultScratch[offset + i];
        }
        const bool startsTargetGroup = commandIndex == 0 ||
            commands[commandIndex - 1].plan.targetKey != plan.targetKey ||
            commands[commandIndex - 1].plan.targetOwner != plan.targetOwner;
        if (startsTargetGroup) {
            receiptTargets[targetGroupIndex] = {plan.targetKey, plan.targetOwner,
                plan.targetUnit, offset, 0};
            ++targetGroupIndex;
        }
        receiptTargets[targetGroupIndex - 1].changeCount += command.candidates.size();
        offset += command.candidates.size();
    }
    receipt = {targetInstanceKey, targetGroupCount, totalChanges};
    return FloatAttributeHistoryStatus::Applied;
}

// Evaluate a plan edit and capture a caller-owned bounded Undo receipt.
// Journal contents are valid only after Applied is returned. Journal and
// scratch spans must not overlap the target, candidates, or input columns.
inline FloatAttributeHistoryStatus evaluateFloatAttributeEditWithReceipt(
    const FloatAttributeEditPlan& plan,
    std::uint64_t targetInstanceKey,
    std::span<const AttributeColumn> columns,
    std::span<float> targetValues,
    std::span<const std::size_t> candidates,
    std::span<float> scratch,
    std::span<FloatAttributeEditChange> journal,
    FloatAttributeEditReceipt& receipt) noexcept {
    if (!detail::validFloatAttributeEditPlan(plan) || targetInstanceKey == 0 ||
        scratch.size() < candidates.size() || journal.size() < candidates.size())
        return !detail::validFloatAttributeEditPlan(plan) || targetInstanceKey == 0
            ? FloatAttributeHistoryStatus::InvalidInput
            : FloatAttributeHistoryStatus::Defer;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto index = candidates[i];
        if (index >= targetValues.size() ||
            (i != 0 && candidates[i - 1] >= index) ||
            !std::isfinite(targetValues[index]))
            return FloatAttributeHistoryStatus::InvalidInput;
        journal[i] = {index, targetValues[index], 0.0f};
    }
    const auto status = evaluateFloatAttributeEdit(
        plan, columns, targetValues, candidates, scratch);
    if (status != AttributeTransferStatus::Ready) {
        if (status == AttributeTransferStatus::Defer)
            return FloatAttributeHistoryStatus::Defer;
        return FloatAttributeHistoryStatus::InvalidInput;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i)
        journal[i].after = targetValues[candidates[i]];
    receipt = {targetInstanceKey, plan.targetKey, plan.targetOwner,
               plan.targetUnit, candidates.size()};
    return FloatAttributeHistoryStatus::Applied;
}

// Restore only if every target value still matches the side of the receipt
// being reversed. Conflict checks finish before any target element is changed.
inline FloatAttributeHistoryStatus applyFloatAttributeEditHistory(
    const FloatAttributeEditReceipt& receipt,
    std::span<const FloatAttributeEditChange> journal,
    std::uint64_t targetInstanceKey,
    std::uint64_t targetKey, AttributeOwner targetOwner,
    AttributeUnit targetUnit,
    std::span<float> targetValues,
    FloatAttributeHistoryDirection direction) noexcept {
    if (targetInstanceKey == 0 || receipt.targetInstanceKey != targetInstanceKey ||
        receipt.targetKey == 0 || receipt.targetKey != targetKey ||
        receipt.targetOwner != targetOwner || receipt.targetUnit != targetUnit ||
        !detail::validAttributeOwner(targetOwner) || !detail::validAttributeUnit(targetUnit) ||
        receipt.changeCount > journal.size() ||
        receipt.changeCount > targetValues.size() ||
        (direction != FloatAttributeHistoryDirection::Undo &&
         direction != FloatAttributeHistoryDirection::Redo))
        return FloatAttributeHistoryStatus::InvalidInput;
    for (std::size_t i = 0; i < receipt.changeCount; ++i) {
        const auto& change = journal[i];
        if (change.elementIndex >= targetValues.size() ||
            (i != 0 && journal[i - 1].elementIndex >= change.elementIndex) ||
            !std::isfinite(change.before) || !std::isfinite(change.after))
            return FloatAttributeHistoryStatus::InvalidInput;
        const float expected = direction == FloatAttributeHistoryDirection::Undo
            ? change.after : change.before;
        if (targetValues[change.elementIndex] != expected)
            return FloatAttributeHistoryStatus::Conflict;
    }
    for (std::size_t i = 0; i < receipt.changeCount; ++i) {
        const auto& change = journal[i];
        targetValues[change.elementIndex] = direction == FloatAttributeHistoryDirection::Undo
            ? change.before : change.after;
    }
    return direction == FloatAttributeHistoryDirection::Undo
        ? FloatAttributeHistoryStatus::Undone
        : FloatAttributeHistoryStatus::Redone;
}

// Undo or redo a multi-target plan batch atomically. Every target and journal
// entry is validated before any mutable column is changed. Mutable target spans
// must be pairwise non-overlapping and match the batch's original identity; the
// caller provides exclusive access for the validation and commit interval.
inline FloatAttributeHistoryStatus applyFloatAttributeEditBatchHistory(
    const FloatAttributeEditBatchReceipt& receipt,
    std::span<const FloatAttributeEditBatchTargetReceipt> receiptTargets,
    std::span<const FloatAttributeEditChange> journal,
    std::uint64_t targetInstanceKey,
    std::span<const FloatAttributeMutableColumn> mutableTargets,
    FloatAttributeHistoryDirection direction) noexcept {
    if (targetInstanceKey == 0 || receipt.targetInstanceKey != targetInstanceKey ||
        receipt.targetCount == 0 || receipt.targetCount > receiptTargets.size() ||
        receipt.targetCount > mutableTargets.size() ||
        receipt.changeCount > journal.size() ||
        (direction != FloatAttributeHistoryDirection::Undo &&
         direction != FloatAttributeHistoryDirection::Redo))
        return FloatAttributeHistoryStatus::InvalidInput;

    std::size_t expectedOffset = 0;
    for (std::size_t targetIndex = 0; targetIndex < receipt.targetCount; ++targetIndex) {
        const auto& targetReceipt = receiptTargets[targetIndex];
        if (targetReceipt.targetKey == 0 ||
            !detail::validAttributeOwner(targetReceipt.targetOwner) ||
            !detail::validAttributeUnit(targetReceipt.targetUnit) ||
            targetReceipt.changeOffset != expectedOffset ||
            targetReceipt.changeCount > receipt.changeCount - expectedOffset)
            return FloatAttributeHistoryStatus::InvalidInput;
        for (std::size_t previous = 0; previous < targetIndex; ++previous)
            if (receiptTargets[previous].targetKey == targetReceipt.targetKey &&
                receiptTargets[previous].targetOwner == targetReceipt.targetOwner)
                return FloatAttributeHistoryStatus::InvalidInput;

        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets) {
            if (candidate.key != targetReceipt.targetKey ||
                candidate.owner != targetReceipt.targetOwner)
                continue;
            if (target) return FloatAttributeHistoryStatus::InvalidInput;
            target = &candidate;
        }
        if (!target || target->unit != targetReceipt.targetUnit)
            return FloatAttributeHistoryStatus::InvalidInput;

        for (std::size_t i = 0; i < targetReceipt.changeCount; ++i) {
            const auto& change = journal[targetReceipt.changeOffset + i];
            if (change.elementIndex >= target->values.size() ||
                (i != 0 && journal[targetReceipt.changeOffset + i - 1].elementIndex >=
                               change.elementIndex) ||
                !std::isfinite(change.before) || !std::isfinite(change.after))
                return FloatAttributeHistoryStatus::InvalidInput;
            const float expected = direction == FloatAttributeHistoryDirection::Undo
                ? change.after : change.before;
            if (target->values[change.elementIndex] != expected)
                return FloatAttributeHistoryStatus::Conflict;
        }
        expectedOffset += targetReceipt.changeCount;
    }
    if (expectedOffset != receipt.changeCount)
        return FloatAttributeHistoryStatus::InvalidInput;

    for (std::size_t targetIndex = 0; targetIndex < receipt.targetCount; ++targetIndex) {
        const auto& targetReceipt = receiptTargets[targetIndex];
        const FloatAttributeMutableColumn* target = nullptr;
        for (const auto& candidate : mutableTargets)
            if (candidate.key == targetReceipt.targetKey &&
                candidate.owner == targetReceipt.targetOwner) {
                target = &candidate;
                break;
            }
        for (std::size_t i = 0; i < targetReceipt.changeCount; ++i) {
            const auto& change = journal[targetReceipt.changeOffset + i];
            target->values[change.elementIndex] = direction == FloatAttributeHistoryDirection::Undo
                ? change.before : change.after;
        }
    }
    return direction == FloatAttributeHistoryDirection::Undo
        ? FloatAttributeHistoryStatus::Undone
        : FloatAttributeHistoryStatus::Redone;
}

// Filter a caller-supplied sorted candidate list by a float column predicate.
// Output indices must not overlap candidates or any attribute value storage.
// The required result count is checked before output is written; on failure,
// output and outputCount remain unchanged.
inline AttributeTransferStatus filterFloatAttribute(
    const AttributeColumn* source, AttributeOwner expectedOwner,
    AttributeUnit expectedUnit, std::span<const std::size_t> candidates,
    FloatAttributeComparison comparison, float operand,
    std::span<std::size_t> output, std::size_t& outputCount) noexcept {
    if (static_cast<unsigned>(expectedOwner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(expectedUnit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        static_cast<unsigned>(comparison) >
            static_cast<unsigned>(FloatAttributeComparison::GreaterOrEqual) ||
        !std::isfinite(operand))
        return AttributeTransferStatus::InvalidInput;
    if (!source) return AttributeTransferStatus::Missing;
    if (source->key == 0 || source->owner != expectedOwner || source->unit != expectedUnit)
        return AttributeTransferStatus::TypeOrUnitMismatch;
    const auto* values = std::get_if<std::span<const float>>(&source->values);
    if (!values) return AttributeTransferStatus::TypeOrUnitMismatch;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i] >= values->size() ||
            (i != 0 && candidates[i - 1] >= candidates[i]))
            return AttributeTransferStatus::InvalidInput;
        if (!std::isfinite((*values)[candidates[i]]))
            return AttributeTransferStatus::InvalidInput;
    }
    const auto matches = [&](float value) noexcept {
        switch (comparison) {
        case FloatAttributeComparison::Equal: return value == operand;
        case FloatAttributeComparison::NotEqual: return value != operand;
        case FloatAttributeComparison::Less: return value < operand;
        case FloatAttributeComparison::LessOrEqual: return value <= operand;
        case FloatAttributeComparison::Greater: return value > operand;
        case FloatAttributeComparison::GreaterOrEqual: return value >= operand;
        }
        return false;
    };
    std::size_t matchCount = 0;
    for (const auto index : candidates)
        if (matches((*values)[index])) ++matchCount;
    if (output.size() < matchCount) return AttributeTransferStatus::Defer;
    std::size_t writeIndex = 0;
    for (const auto index : candidates)
        if (matches((*values)[index])) output[writeIndex++] = index;
    outputCount = matchCount;
    return AttributeTransferStatus::Ready;
}

// Assign one constant value to the selected elements of a caller-owned typed
// column. This is the low-level authoring primitive for a future Houdini-like
// attribute node; schema creation and persistence remain cold-path adapter work.
// Selection must contain unique, strictly increasing element indices. The
// column must already have elementCount entries. Validation is completed before
// writing so invalid requests never partially modify the column.
template<class T>
AttributeTransferStatus assignAttribute(
    std::span<T> values, std::size_t elementCount,
    std::span<const std::size_t> selectedIndices,
    const T& assignedValue) noexcept {
    if (values.size() != elementCount) return AttributeTransferStatus::InvalidInput;
    for (std::size_t i = 0; i < selectedIndices.size(); ++i) {
        if (selectedIndices[i] >= elementCount ||
            (i != 0 && selectedIndices[i - 1] >= selectedIndices[i]))
            return AttributeTransferStatus::InvalidInput;
    }
    const T value = assignedValue;
    for (const auto index : selectedIndices) values[index] = value;
    return AttributeTransferStatus::Ready;
}

// Apply a scalar operation to selected float values. The caller supplies a
// scratch span at least as large as the selection; it must not overlap values
// or selectedIndices. The entire result is calculated and checked for finite
// values before the destination column is changed.
inline AttributeTransferStatus applyFloatAttribute(
    std::span<float> values, std::size_t elementCount,
    std::span<const std::size_t> selectedIndices,
    FloatAttributeOperation operation, float operand,
    std::span<float> scratch) noexcept {
    if (values.size() != elementCount || !std::isfinite(operand) ||
        static_cast<unsigned>(operation) >
            static_cast<unsigned>(FloatAttributeOperation::Maximum))
        return AttributeTransferStatus::InvalidInput;
    for (std::size_t i = 0; i < selectedIndices.size(); ++i) {
        if (selectedIndices[i] >= elementCount ||
            (i != 0 && selectedIndices[i - 1] >= selectedIndices[i]))
            return AttributeTransferStatus::InvalidInput;
    }
    if (scratch.size() < selectedIndices.size()) return AttributeTransferStatus::Defer;
    for (std::size_t i = 0; i < selectedIndices.size(); ++i) {
        const float current = values[selectedIndices[i]];
        if (!std::isfinite(current)) return AttributeTransferStatus::InvalidInput;
        float result = 0.0f;
        switch (operation) {
        case FloatAttributeOperation::Set: result = operand; break;
        case FloatAttributeOperation::Add: result = current + operand; break;
        case FloatAttributeOperation::Subtract: result = current - operand; break;
        case FloatAttributeOperation::Multiply: result = current * operand; break;
        case FloatAttributeOperation::Divide:
            if (operand == 0.0f) return AttributeTransferStatus::InvalidInput;
            result = current / operand;
            break;
        case FloatAttributeOperation::Minimum:
            result = current < operand ? current : operand;
            break;
        case FloatAttributeOperation::Maximum:
            result = current > operand ? current : operand;
            break;
        }
        if (!std::isfinite(result)) return AttributeTransferStatus::InvalidInput;
        scratch[i] = result;
    }
    for (std::size_t i = 0; i < selectedIndices.size(); ++i)
        values[selectedIndices[i]] = scratch[i];
    return AttributeTransferStatus::Ready;
}

// Combine a writable float column with a compatible borrowed float column.
// Both descriptors must use the same owner and unit. Multiplication and
// division are excluded because their result units need a richer unit system.
// Source and destination may alias; scratch must be disjoint from both.
inline AttributeTransferStatus combineFloatAttribute(
    std::span<float> destination, const AttributeColumn* source,
    AttributeOwner expectedOwner, AttributeUnit expectedUnit,
    std::span<const std::size_t> selectedIndices,
    FloatAttributeOperation operation, std::span<float> scratch) noexcept {
    if (static_cast<unsigned>(expectedOwner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(expectedUnit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        (operation != FloatAttributeOperation::Set &&
         operation != FloatAttributeOperation::Add &&
         operation != FloatAttributeOperation::Subtract &&
         operation != FloatAttributeOperation::Minimum &&
         operation != FloatAttributeOperation::Maximum))
        return AttributeTransferStatus::InvalidInput;
    if (!source) return AttributeTransferStatus::Missing;
    if (source->key == 0 || source->owner != expectedOwner || source->unit != expectedUnit)
        return AttributeTransferStatus::TypeOrUnitMismatch;
    const auto* sourceValues = std::get_if<std::span<const float>>(&source->values);
    if (!sourceValues) return AttributeTransferStatus::TypeOrUnitMismatch;
    if (sourceValues->size() != destination.size()) return AttributeTransferStatus::InvalidInput;
    for (std::size_t i = 0; i < selectedIndices.size(); ++i) {
        if (selectedIndices[i] >= destination.size() ||
            (i != 0 && selectedIndices[i - 1] >= selectedIndices[i]))
            return AttributeTransferStatus::InvalidInput;
    }
    if (scratch.size() < selectedIndices.size()) return AttributeTransferStatus::Defer;
    for (std::size_t i = 0; i < selectedIndices.size(); ++i) {
        const auto index = selectedIndices[i];
        const float left = destination[index];
        const float right = (*sourceValues)[index];
        if (!std::isfinite(left) || !std::isfinite(right))
            return AttributeTransferStatus::InvalidInput;
        float result = 0.0f;
        switch (operation) {
        case FloatAttributeOperation::Set: result = right; break;
        case FloatAttributeOperation::Add: result = left + right; break;
        case FloatAttributeOperation::Subtract: result = left - right; break;
        case FloatAttributeOperation::Minimum: result = left < right ? left : right; break;
        case FloatAttributeOperation::Maximum: result = left > right ? left : right; break;
        case FloatAttributeOperation::Multiply:
        case FloatAttributeOperation::Divide: return AttributeTransferStatus::InvalidInput;
        }
        if (!std::isfinite(result)) return AttributeTransferStatus::InvalidInput;
        scratch[i] = result;
    }
    for (std::size_t i = 0; i < selectedIndices.size(); ++i)
        destination[selectedIndices[i]] = scratch[i];
    return AttributeTransferStatus::Ready;
}

// Create a missing schema column without allocating. The caller owns the
// descriptor and value storage, and publishes the descriptor only after this
// function succeeds. Existing keys in the same owner domain are rejected.
template<class T>
AttributeTransferStatus createAttributeColumn(
    std::span<AttributeColumn> columns, std::uint64_t key,
    AttributeOwner owner, AttributeUnit unit, std::span<T> values,
    std::size_t elementCount, AttributeColumn& created) noexcept {
    if (key == 0 || static_cast<unsigned>(owner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(unit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        values.size() != elementCount)
        return AttributeTransferStatus::InvalidInput;
    for (const auto& column : columns) {
        if (column.key == key && column.owner == owner)
            return AttributeTransferStatus::AlreadyExists;
    }
    created = {key, owner, unit, std::span<const T>{values}};
    return AttributeTransferStatus::Ready;
}

// Source indices explicitly map each destination element to its source element.
// Repeated indices deliberately support one fragment producing many dust samples.
// Output must not overlap source values or indices. All storage is caller-owned.
// This transfers one column atomically, not an entire multi-column transaction.
template<class T>
AttributeTransferStatus inheritAttribute(
    const AttributeColumn* source, AttributeOwner expectedOwner,
    AttributeUnit expectedUnit, std::span<const std::size_t> sourceIndices,
    std::span<T> output, MissingAttribute missing, const T& defaultValue) noexcept {
    if (static_cast<unsigned>(expectedOwner) > static_cast<unsigned>(AttributeOwner::Layer) ||
        static_cast<unsigned>(expectedUnit) > static_cast<unsigned>(AttributeUnit::Meters) ||
        (missing != MissingAttribute::Reject && missing != MissingAttribute::UseDefault))
        return AttributeTransferStatus::InvalidInput;
    const std::span<const T>* values = nullptr;
    if (source) {
        if (source->key == 0 || source->owner != expectedOwner)
            return AttributeTransferStatus::InvalidInput;
        values = std::get_if<std::span<const T>>(&source->values);
        if (!values || source->unit != expectedUnit)
            return AttributeTransferStatus::TypeOrUnitMismatch;
        for (const auto index : sourceIndices) {
            if (index >= values->size()) return AttributeTransferStatus::InvalidInput;
        }
    } else if (missing == MissingAttribute::Reject) {
        return AttributeTransferStatus::Missing;
    }
    if (output.size() < sourceIndices.size()) return AttributeTransferStatus::Defer;
    // Snapshot the default before output writes; it may refer to an output slot.
    const T fallback = defaultValue;
    for (std::size_t i = 0; i < sourceIndices.size(); ++i)
        output[i] = values ? (*values)[sourceIndices[i]] : fallback;
    return AttributeTransferStatus::Ready;
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
