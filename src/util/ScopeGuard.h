#pragma once

#include <utility>

namespace yave {

/// スコープ終了時に確実に処理を実行する RAII ユーティリティ。
///
///   auto guard = ScopeGuard([&] { resource.release(); });
///
class ScopeGuard final
{
public:
    template <typename F>
    explicit ScopeGuard(F&& fn) : fn_(std::forward<F>(fn)), active_(true) {}

    ScopeGuard(ScopeGuard&& o) noexcept
        : fn_(std::move(o.fn_)), active_(o.active_) { o.active_ = false; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

    ~ScopeGuard()
    {
        if (active_)
            fn_();
    }

    void dismiss() { active_ = false; }

private:
    std::function<void()> fn_;
    bool                  active_;
};

} // namespace yave
