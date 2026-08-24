module;

#include <QString>
#include <QUuid>
#include <QJsonObject>

export module Collaborate.Review;

// Re-exported: CollabOperationData appears in this module's public API.
export import Collaborate.Session;
export import Core.ArtifactArray;

export namespace ArtifactCore {

// ---- durable comments anchored to stable IDs (composition/layer/frame) ----

struct CollabComment {
    QString commentId;
    QString parentCommentId; // empty = top-level thread root
    QString authorClientId;
    QString authorUserId;
    QString authorName;
    QString compositionId;
    QString layerId;      // optional anchor; empty = composition-level
    qint64 frame = -1;    // optional timeline anchor; -1 = none
    QString text;
    qint64 createdAtMs = 0;
    bool resolved = false;

    [[nodiscard]] bool isReply() const noexcept { return !parentCommentId.isEmpty(); }
};

enum class CollabProposalStatus {
    Pending,
    Accepted,
    Rejected,
    Withdrawn,
};

[[nodiscard]] inline QString collabProposalStatusName(const CollabProposalStatus status) noexcept {
    switch (status) {
    case CollabProposalStatus::Pending:   return QStringLiteral("pending");
    case CollabProposalStatus::Accepted:  return QStringLiteral("accepted");
    case CollabProposalStatus::Rejected:  return QStringLiteral("rejected");
    case CollabProposalStatus::Withdrawn: return QStringLiteral("withdrawn");
    }
    return QStringLiteral("pending");
}

// ---- explicit review proposals (apply/reject lifecycle) ----
//
// A proposal bundles semantic operations for review. The session model stays
// authoritative; accepting a proposal only returns its operations — the
// caller decides whether to apply them locally and/or broadcast them.

struct CollabProposal {
    QString proposalId;
    QString title;
    QString authorClientId;
    QString authorName;
    Array<CollabOperationData> operations;
    CollabProposalStatus status = CollabProposalStatus::Pending;
    qint64 createdAtMs = 0;
    qint64 decidedAtMs = 0;
    QString decidedByClientId;
    QString decisionReason;
};

[[nodiscard]] inline QString newCollabId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

class CollaborationReview {
public:
    // ---- comments ----

    [[nodiscard]] QString addComment(const QString& authorClientId,
                                     const QString& authorUserId,
                                     const QString& authorName,
                                     const QString& compositionId,
                                     const QString& layerId, const qint64 frame,
                                     const QString& text, const qint64 atMs) {
        if (text.trimmed().isEmpty()) return {};
        CollabComment comment;
        comment.commentId = newCollabId();
        comment.authorClientId = authorClientId;
        comment.authorUserId = authorUserId;
        comment.authorName = authorName;
        comment.compositionId = compositionId;
        comment.layerId = layerId;
        comment.frame = frame;
        comment.text = text.trimmed();
        comment.createdAtMs = atMs;
        comments_.append(comment);
        return comment.commentId;
    }

    [[nodiscard]] QString addReply(const QString& parentCommentId,
                                   const QString& authorClientId,
                                   const QString& authorUserId,
                                   const QString& authorName,
                                   const QString& text, const qint64 atMs) {
        const auto parent = find(parentCommentId);
        if (!parent) return {};
        // Replies inherit the thread anchor and cannot nest.
        if (parent->isReply()) return {};
        if (text.trimmed().isEmpty()) return {};
        CollabComment reply;
        reply.commentId = newCollabId();
        reply.parentCommentId = parentCommentId;
        reply.authorClientId = authorClientId;
        reply.authorUserId = authorUserId;
        reply.authorName = authorName;
        reply.compositionId = parent->compositionId;
        reply.layerId = parent->layerId;
        reply.frame = parent->frame;
        reply.text = text.trimmed();
        reply.createdAtMs = atMs;
        comments_.append(reply);
        return reply.commentId;
    }

    bool resolveComment(const QString& commentId) {
        const auto comment = find(commentId);
        if (!comment || comment->isReply()) return false;
        comment->resolved = true;
        return true;
    }

    bool reopenComment(const QString& commentId) {
        const auto comment = find(commentId);
        if (!comment) return false;
        comment->resolved = false;
        return true;
    }

    bool editComment(const QString& commentId, const QString& newText,
                     const QString& editorClientId) {
        const auto comment = find(commentId);
        if (!comment) return false;
        if (comment->authorClientId != editorClientId) return false;
        if (newText.trimmed().isEmpty()) return false;
        comment->text = newText.trimmed();
        return true;
    }

    bool removeComment(const QString& commentId,
                       const QString& requesterClientId) {
        for (int i = 0; i < comments_.size(); ++i) {
            const auto& comment = comments_[i];
            const bool isOwner = comment.authorClientId == requesterClientId;
            if (comment.commentId == commentId && isOwner) {
                // Remove the thread root together with its replies.
                comments_.removeAt(i);
                for (int j = comments_.size() - 1; j >= 0; --j) {
                    if (comments_[j].parentCommentId == commentId) {
                        comments_.removeAt(j);
                    }
                }
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] Array<CollabComment> commentsFor(
        const QString& compositionId, const QString& layerId = {},
        const bool includeResolved = false) const {
        Array<CollabComment> result;
        for (const auto& comment : comments_) {
            if (comment.compositionId != compositionId) continue;
            if (!layerId.isEmpty() && comment.layerId != layerId) continue;
            if (!includeResolved && comment.resolved) continue;
            result.append(comment);
        }
        return result;
    }

    [[nodiscard]] Array<CollabComment> repliesFor(const QString& parentCommentId) const {
        Array<CollabComment> result;
        for (const auto& comment : comments_) {
            if (comment.parentCommentId == parentCommentId) result.append(comment);
        }
        return result;
    }

    [[nodiscard]] int unresolvedCount() const {
        int total = 0;
        for (const auto& comment : comments_) {
            if (!comment.resolved && !comment.isReply()) ++total;
        }
        return total;
    }

    // ---- proposals ----

    [[nodiscard]] QString createProposal(const QString& title,
                                         const QString& authorClientId,
                                         const QString& authorName,
                                         const Array<CollabOperationData>& operations,
                                         const qint64 atMs) {
        if (operations.isEmpty() || title.trimmed().isEmpty()) return {};
        CollabProposal proposal;
        proposal.proposalId = newCollabId();
        proposal.title = title.trimmed();
        proposal.authorClientId = authorClientId;
        proposal.authorName = authorName;
        proposal.operations = operations;
        proposal.createdAtMs = atMs;
        proposals_.append(proposal);
        return proposal.proposalId;
    }

    // Accepting returns the bundled operations for the caller to apply and
    // broadcast; the proposal itself never mutates project state directly.
    [[nodiscard]] Array<CollabOperationData> acceptProposal(
        const QString& proposalId, const QString& decidedByClientId,
        const qint64 atMs, const QString& reason = {}) {
        auto proposal = findProposal(proposalId);
        if (!proposal || proposal->status != CollabProposalStatus::Pending) {
            return {};
        }
        proposal->status = CollabProposalStatus::Accepted;
        proposal->decidedAtMs = atMs;
        proposal->decidedByClientId = decidedByClientId;
        proposal->decisionReason = reason;
        return proposal->operations;
    }

    bool rejectProposal(const QString& proposalId,
                        const QString& decidedByClientId, const qint64 atMs,
                        const QString& reason = {}) {
        auto proposal = findProposal(proposalId);
        if (!proposal || proposal->status != CollabProposalStatus::Pending) {
            return false;
        }
        proposal->status = CollabProposalStatus::Rejected;
        proposal->decidedAtMs = atMs;
        proposal->decidedByClientId = decidedByClientId;
        proposal->decisionReason = reason;
        return true;
    }

    bool withdrawProposal(const QString& proposalId,
                          const QString& authorClientId, const qint64 atMs) {
        auto proposal = findProposal(proposalId);
        if (!proposal ||
            proposal->status != CollabProposalStatus::Pending ||
            proposal->authorClientId != authorClientId) {
            return false;
        }
        proposal->status = CollabProposalStatus::Withdrawn;
        proposal->decidedAtMs = atMs;
        return true;
    }

    [[nodiscard]] CollabProposal proposal(const QString& proposalId) const {
        for (const auto& proposal : proposals_) {
            if (proposal.proposalId == proposalId) return proposal;
        }
        return {};
    }

    [[nodiscard]] Array<CollabProposal> pendingProposals() const {
        Array<CollabProposal> result;
        for (const auto& proposal : proposals_) {
            if (proposal.status == CollabProposalStatus::Pending) {
                result.append(proposal);
            }
        }
        return result;
    }

private:
    [[nodiscard]] CollabComment* find(const QString& commentId) {
        for (auto& comment : comments_) {
            if (comment.commentId == commentId) return &comment;
        }
        return nullptr;
    }
    [[nodiscard]] const CollabComment* find(const QString& commentId) const {
        for (const auto& comment : comments_) {
            if (comment.commentId == commentId) return &comment;
        }
        return nullptr;
    }
    [[nodiscard]] CollabProposal* findProposal(const QString& proposalId) {
        for (auto& proposal : proposals_) {
            if (proposal.proposalId == proposalId) return &proposal;
        }
        return nullptr;
    }

    Array<CollabComment> comments_;
    Array<CollabProposal> proposals_;
};

} // namespace ArtifactCore
