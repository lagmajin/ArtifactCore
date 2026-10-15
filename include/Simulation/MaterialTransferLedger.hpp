#pragma once

#include "MaterialTransfer.hpp"

namespace ArtifactCore::Experimental::MaterialTransfer {

struct Occurrence {
    std::uint64_t connectionId = 0;
    std::uint64_t occurrenceId = 0;
    std::int64_t tick = 0;
    bool operator==(const Occurrence&) const = default;
};

enum class LedgerStatus : std::uint8_t { Available, Duplicate, Defer, Invalid };

// Single-owner, bounded ledger. Storage is supplied by caller, never overwritten.
// Retain until checkpoint/history retirement; no automatic eviction.
// This records committed batches only; solver transaction remains adapter-owned.
class TransferLedger {
public:
    explicit TransferLedger(std::span<Occurrence> storage) noexcept : storage_(storage) {}
    TransferLedger(const TransferLedger&) = delete;
    TransferLedger& operator=(const TransferLedger&) = delete;

    LedgerStatus probe(Occurrence key) const noexcept {
        if (key.connectionId == 0 || key.occurrenceId == 0) return LedgerStatus::Invalid;
        for (std::size_t i = 0; i < size_; ++i) {
            if (storage_[i] == key) return LedgerStatus::Duplicate;
        }
        return size_ == storage_.size() ? LedgerStatus::Defer : LedgerStatus::Available;
    }

    // Caller probes before solver commit, then records without interleaved writes.
    LedgerStatus recordCommitted(Occurrence key) noexcept {
        const auto status = probe(key);
        if (status == LedgerStatus::Available) storage_[size_++] = key;
        return status;
    }

    std::span<const Occurrence> checkpoint() const noexcept {
        return storage_.first(size_);
    }

    // Cold path: rejects invalid/duplicate input before changing any state.
    // Source must be disjoint from storage, or exactly checkpoint() of this ledger.
    bool restore(std::span<const Occurrence> saved) noexcept {
        if (saved.size() > storage_.size()) return false;
        for (std::size_t i = 0; i < saved.size(); ++i) {
            if (saved[i].connectionId == 0 || saved[i].occurrenceId == 0) return false;
            for (std::size_t j = 0; j < i; ++j) {
                if (saved[i] == saved[j]) return false;
            }
        }
        for (std::size_t i = 0; i < saved.size(); ++i) storage_[i] = saved[i];
        size_ = saved.size();
        return true;
    }

private:
    std::span<Occurrence> storage_;
    std::size_t size_ = 0;
};

} // namespace ArtifactCore::Experimental::MaterialTransfer
