module;

#include <atomic>
#include <cstdint>
#include <source_location>
#include <utility>
#include <QString>

export module Core.Diagnostics.DebugIdentity;

namespace Artifact::Diagnostics {

export class DebugIdentity
{
public:
    explicit DebugIdentity(
        QString typeName = {},
        const std::source_location location = std::source_location::current())
        : id_(nextId()),
          typeName_(std::move(typeName)),
          creationFile_(QString::fromUtf8(location.file_name())),
          creationFunction_(QString::fromUtf8(location.function_name())),
          creationLine_(location.line())
    {
    }

    DebugIdentity(const DebugIdentity&) = delete;
    DebugIdentity& operator=(const DebugIdentity&) = delete;
    DebugIdentity(DebugIdentity&&) = delete;
    DebugIdentity& operator=(DebugIdentity&&) = delete;
    virtual ~DebugIdentity() = default;

    uint64_t debugId() const noexcept { return id_; }

    const QString& debugName() const noexcept { return debugName_; }
    void setDebugName(QString name) { debugName_ = std::move(name); }

    const QString& debugTypeName() const noexcept { return typeName_; }

    void setDebugOwner(uint64_t ownerId, QString ownerName = {})
    {
        ownerId_ = ownerId;
        ownerName_ = std::move(ownerName);
    }

    void setDebugOwner(const DebugIdentity& owner)
    {
        setDebugOwner(owner.debugId(), owner.debugLabel());
    }

    uint64_t debugOwnerId() const noexcept { return ownerId_; }
    const QString& debugOwnerName() const noexcept { return ownerName_; }

    const QString& creationFile() const noexcept { return creationFile_; }
    const QString& creationFunction() const noexcept { return creationFunction_; }
    uint32_t creationLine() const noexcept { return creationLine_; }

    QString debugLabel() const
    {
        QString label = typeName_.isEmpty() ? QStringLiteral("Object") : typeName_;
        label += QStringLiteral("#") + QString::number(id_);
        if (!debugName_.isEmpty())
            label += QStringLiteral("(\"") + debugName_ + QStringLiteral("\")");
        return label;
    }

private:
    static uint64_t nextId() noexcept
    {
        return nextId_.fetch_add(1, std::memory_order_relaxed);
    }

    inline static std::atomic<uint64_t> nextId_{1};
    const uint64_t id_;
    QString typeName_;
    QString debugName_;
    uint64_t ownerId_{0};
    QString ownerName_;
    QString creationFile_;
    QString creationFunction_;
    uint32_t creationLine_{0};
};

} // namespace Artifact::Diagnostics
