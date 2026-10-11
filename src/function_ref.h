#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace headsup
{
    // A callable handed down a call without copying or allocating, as C++26's std::function_ref: it refers to the
    // caller's callable, so it is a parameter and never kept past the call.
    template <typename Signature>
    class FunctionRef;

    template <typename Result, typename... Args>
    class FunctionRef<Result(Args...)>
    {
    public:
        template <typename Callable>
            requires(!std::is_same_v<std::remove_cvref_t<Callable>, FunctionRef> &&
                     std::is_invocable_r_v<Result, Callable&, Args...>)
        FunctionRef(Callable&& callable)
            : m_Callable(reinterpret_cast<std::intptr_t>(std::addressof(callable))),
              m_Call([](std::intptr_t c, Args... args) -> Result {
                  // A function as well as an object: the address is kept as a number either way.
                  using Pointer = std::add_pointer_t<std::remove_reference_t<Callable>>;
                  return std::invoke(*reinterpret_cast<Pointer>(c), std::forward<Args>(args)...);
              })
        {
        }

        Result operator()(Args... args) const { return m_Call(m_Callable, std::forward<Args>(args)...); }

    private:
        std::intptr_t m_Callable;
        Result (*m_Call)(std::intptr_t, Args...);
    };
}
