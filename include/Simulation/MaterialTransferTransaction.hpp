#pragma once

#include "MaterialTransferLedger.hpp"

namespace ArtifactCore::Experimental::MaterialTransfer {

enum class CommitStatus : std::uint8_t {
    Committed, AlreadyCommitted, InvalidInput, SpaceMismatch, Defer
};

// Reference transaction over caller-owned CPU storage, not a solver adapter.
// Single owner: no concurrent observers, callbacks or reentrant operations.
// All spans, ledger storage and targetSize must be mutually disjoint.
// Selected indices and their source IDs must both be strictly increasing.
// Ledger key identifies an immutable request AND selection across retries.
// Target entries retain source IDs; destination solver IDs are allocated later.
// Source/target/ledger must be checkpointed together for replay.
inline CommitStatus commitConversion(
    const Request& request, std::span<const Sample> source,
    std::span<bool> sourceActive, std::span<const std::size_t> selected,
    std::span<Spawn> target, std::size_t& targetSize,
    TransferLedger& ledger) noexcept {
    const auto validation = prepare(request, {}, {});
    if (validation.status == Status::SpaceMismatch) return CommitStatus::SpaceMismatch;
    if (validation.status != Status::Ready || request.mode != Mode::Convert ||
        source.size() != sourceActive.size() || targetSize > target.size())
        return CommitStatus::InvalidInput;
    const Occurrence key{request.connectionId, request.occurrenceId, request.tick};
    const auto ledgerState = ledger.probe(key);
    // Duplicate requests may refer to source elements already consumed by us.
    if (ledgerState == LedgerStatus::Duplicate) return CommitStatus::AlreadyCommitted;
    if (ledgerState == LedgerStatus::Invalid) return CommitStatus::InvalidInput;
    for (std::size_t i = 0; i < selected.size(); ++i) {
        const auto index = selected[i];
        if (index >= source.size() || (i != 0 && selected[i - 1] >= index) ||
            !sourceActive[index] || !valid(source[index])) return CommitStatus::InvalidInput;
        if (i != 0 && source[selected[i - 1]].id >= source[index].id)
            return CommitStatus::InvalidInput;
    }
    if (ledgerState == LedgerStatus::Defer || selected.size() > target.size() - targetSize)
        return CommitStatus::Defer;

    // From here all operations are non-throwing value assignments. Exclusive
    // ownership makes the state transition observable only upon return.
    if (ledger.recordCommitted(key) != LedgerStatus::Available) return CommitStatus::Defer;
    for (std::size_t i = 0; i < selected.size(); ++i) {
        target[targetSize + i] = {request.connectionId, request.occurrenceId,
                                 request.tick, source[selected[i]]};
    }
    for (const auto index : selected) sourceActive[index] = false;
    targetSize += selected.size();
    return CommitStatus::Committed;
}

} // namespace ArtifactCore::Experimental::MaterialTransfer
