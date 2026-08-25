#pragma once

#include "../core/Rational.h"
#include <QString>
#include <optional>

namespace yave {

/// タイムコードのフォーマット (10.8 ロケール依存フォーマット)。
/// ロケール非依存。常に HH:MM:SS:FF (ドロップフレーム時は HH:MM:SS;FF)。
QString formatTimecode(int64_t frame, const Rational& tb, bool dropFrame);

/// タイムコードのパース。
std::optional<int64_t> parseTimecode(const QString& s, const Rational& tb);

} // namespace yave
