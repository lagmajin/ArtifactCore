#pragma once

#include "MaterialTransfer.hpp"
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
    Ready, Missing, TypeOrUnitMismatch, InvalidInput, Defer
};

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
