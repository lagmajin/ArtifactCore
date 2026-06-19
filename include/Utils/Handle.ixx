module;
#include <cstdint>
#include <utility>
#include <QHashFunctions>

export module Utils.Handle;

export namespace ArtifactCore {

template <typename Tag>
class Handle {
 public:
  using value_type = std::uint64_t;

  constexpr Handle() = default;
  constexpr explicit Handle(value_type value) : value_(value) {}

  static constexpr Handle fromValue(value_type value) noexcept { return Handle(value); }
  static constexpr Handle FromNil() noexcept { return Nil(); }

  static constexpr Handle Nil() noexcept { return Handle{}; }

  constexpr value_type value() const noexcept { return value_; }
  constexpr value_type raw() const noexcept { return value_; }
  constexpr bool isValid() const noexcept { return value_ != 0; }
  constexpr bool isNil() const noexcept { return value_ == 0; }
  constexpr explicit operator bool() const noexcept { return isValid(); }

  constexpr bool operator==(const Handle&) const noexcept = default;
  constexpr auto operator<=>(const Handle&) const noexcept = default;

 private:
  value_type value_{0};
};

struct HandleTagAsset {};
struct HandleTagProject {};
struct HandleTagLayer {};

using AssetHandle = Handle<HandleTagAsset>;
using ProjectHandle = Handle<HandleTagProject>;
using LayerHandle = Handle<HandleTagLayer>;

template <typename Tag>
using HandleHash = std::uint64_t;

template <typename Tag>
using Hash = HandleHash<Tag>;

template <typename Tag>
inline bool isValid(const Handle<Tag>& handle) noexcept
{
 return handle.isValid();
}

template <typename Tag>
inline uint qHash(const Handle<Tag>& handle, uint seed = 0) noexcept
{
 return qHash(handle.value(), seed);
}

template <typename Tag>
inline bool operator!=(const Handle<Tag>& lhs, std::nullptr_t) noexcept
{
 return lhs.isValid();
}

template <typename Tag>
inline bool operator==(const Handle<Tag>& lhs, std::nullptr_t) noexcept
{
 return lhs.isNil();
}

}
