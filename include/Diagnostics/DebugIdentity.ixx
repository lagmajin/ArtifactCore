module;

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <source_location>
#include <utility>
#include <vector>
#include <mutex>
#include <QString>

export module Core.Diagnostics.DebugIdentity;

import Container.NamedVector;

namespace Artifact::Diagnostics {

export struct DebugIdentitySnapshot
{
    uint64_t id{0};
    uint64_t ownerId{0};
    QString typeName;
    QString name;
    QString ownerName;
    QString creationFile;
    QString creationFunction;
    uint32_t creationLine{0};
};

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
        std::lock_guard lock(registryMutex());
        registry().push_back(this);
    }

    DebugIdentity(const DebugIdentity&) = delete;
    DebugIdentity& operator=(const DebugIdentity&) = delete;
    DebugIdentity(DebugIdentity&&) = delete;
    DebugIdentity& operator=(DebugIdentity&&) = delete;
    virtual ~DebugIdentity()
    {
        std::lock_guard lock(registryMutex());
        auto& entries = registry();
        entries.erase(std::remove(entries.begin(), entries.end(), this), entries.end());
    }

    static std::vector<DebugIdentitySnapshot> snapshotAll()
    {
        std::lock_guard lock(registryMutex());
        std::vector<DebugIdentitySnapshot> result;
        result.reserve(registry().size());
        for (const auto* identity : registry()) {
            result.push_back(DebugIdentitySnapshot{
                identity->debugId(), identity->debugOwnerId(), identity->debugTypeName(),
                identity->debugName(), identity->debugOwnerName(), identity->creationFile(),
                identity->creationFunction(), identity->creationLine()});
        }
        return result;
    }

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
    static std::vector<DebugIdentity*>& registry()
    {
        static std::vector<DebugIdentity*> entries;
        return entries;
    }

    static std::mutex& registryMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

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
