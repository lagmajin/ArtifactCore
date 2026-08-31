module;

#include <cstdlib>
#include <cstddef>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

export module Core.ArtifactFunctionRef;

export namespace ArtifactCore {

template<typename Signature>
class ArtifactFunctionRef;

template<typename R, typename... Args>
class ArtifactFunctionRef<R(Args...)> {
public:
    ArtifactFunctionRef() noexcept = default;
    ArtifactFunctionRef(std::nullptr_t) noexcept {}

    ArtifactFunctionRef(R (*function)(Args...)) noexcept
        : storage_(reinterpret_cast<const void*>(function)),
          invoke_([](const void* storage, Args... args) -> R {
              const auto functionPtr = reinterpret_cast<R (*)(Args...)>(
                  const_cast<void*>(storage));
              return functionPtr(std::forward<Args>(args)...);
          }) {}

    template<typename F>
        requires (!std::same_as<std::remove_cvref_t<F>, ArtifactFunctionRef> &&
                  std::is_invocable_r_v<R, F&, Args...>)
    ArtifactFunctionRef(F& callable) noexcept
        : storage_(std::addressof(callable)),
          invoke_([](const void* storage, Args... args) -> R {
              return (*static_cast<F*>(const_cast<void*>(storage)))(
                  std::forward<Args>(args)...);
          }) {}

    [[nodiscard]] bool isValid() const noexcept { return invoke_ != nullptr; }
    explicit operator bool() const noexcept { return isValid(); }
    void clear() noexcept { storage_ = nullptr; invoke_ = nullptr; }

    R invoke(Args... args) const {
        if (!invoke_) std::abort();
        return invoke_(storage_, std::forward<Args>(args)...);
    }

    R operator()(Args... args) const {
        return invoke(std::forward<Args>(args)...);
    }

private:
    const void* storage_ = nullptr;
    R (*invoke_)(const void*, Args...) = nullptr;
};

} // namespace ArtifactCore
