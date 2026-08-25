#pragma once

namespace yave {

/// 例外を使わない層のための Result 型。
/// エラーはコード + メッセージ。ok() を必ず先に確認すること。
template <typename T, typename E = int>
class Result
{
public:
    static Result ok(T value) { return Result(true, std::move(value), E{}); }
    static Result err(E error) { return Result(false, T{}, std::move(error)); }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }

    /// 値の取り出し。ok() が false の場合は未定義。
    const T& value() const& { return value_; }
    T&&      takeValue() && { return std::move(value_); }
    const E& error() const& { return error_; }

private:
    Result(bool ok, T value, E error)
        : ok_(ok), value_(std::move(value)), error_(std::move(error)) {}

    bool ok_;
    T    value_;
    E    error_;
};

/// void 特殊化
template <typename E>
class Result<void, E>
{
public:
    static Result ok() { return Result(true, E{}); }
    static Result err(E error) { return Result(false, std::move(error)); }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const E& error() const& { return error_; }

private:
    explicit Result(bool ok, E error) : ok_(ok), error_(std::move(error)) {}
    bool ok_;
    E    error_;
};

} // namespace yave
