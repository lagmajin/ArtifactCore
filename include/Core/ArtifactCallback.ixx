module;
#include <functional>
#include <utility>

export module Core.ArtifactCallback;

export namespace ArtifactCore {

/// Type-safe callback. Null-safe: isValid() must be checked, or use ifValid().
template <typename Signature>
class Callback {
public:
    Callback() = default;
    Callback(std::function<Signature> fn) : fn_(std::move(fn)) {}
    Callback(const Callback&) = default;
    Callback(Callback&&) = default;

    bool isValid() const { return static_cast<bool>(fn_); }
    explicit operator bool() const { return isValid(); }

    template <typename... Args>
    auto invoke(Args&&... args) -> decltype(auto) {
        return fn_(std::forward<Args>(args)...);
    }

    void clear() { fn_ = nullptr; }
    void reset(std::function<Signature> fn) { fn_ = std::move(fn); }

private:
    std::function<Signature> fn_;
};

using Action = Callback<void()>;

} // namespace ArtifactCore
