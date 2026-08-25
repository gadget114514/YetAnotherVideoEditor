#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include <array>
#include <vector>

namespace yave {

/// クリップに積むビデオフィルタ 1 段 (3.9)。
///
/// SubtitleEffectInstance と違い、プラグインのインスタンスを所有しない。
/// 組み込みフィルタはシェーダで完結し、状態を持たないためである。
struct VideoFilterInstance
{
    QString     filterId;          ///< "yave.filter.blur" / "aviutl:<nativeId>"
    QVariantMap params;            ///< parameterSchema に沿った値
    bool        enabled = true;

    QJsonObject toJson() const;
    static VideoFilterInstance fromJson(const QJsonObject& o);
};

/// 組み込みフィルタの ID (3.9.2)。
namespace builtinFilter {
inline constexpr auto kColorAdjust = "yave.filter.colorAdjust";
inline constexpr auto kBlur        = "yave.filter.blur";
inline constexpr auto kMono        = "yave.filter.mono";
inline constexpr auto kSepia       = "yave.filter.sepia";
} // namespace builtinFilter

/// 組み込みフィルタ 1 種の記述。ライブラリの一覧表示と既定値の解決に使う。
struct VideoFilterDesc
{
    QString filterId;
    QString displayNameKey;        ///< 翻訳キー (10章)。生の表示文字列は持たない
    QVariantMap defaultParams;
};

/// 組み込みフィルタの一覧。ライブラリパネル (1.7.5) が並べる。
const std::vector<VideoFilterDesc>& builtinVideoFilters();

/// filterId から記述を引く。未知なら nullptr。
const VideoFilterDesc* findVideoFilterDesc(const QString& filterId);

/// QVariantMap のパラメータを、Render Thread へ渡す固定長 float 配列へ潰す (3.9.2)。
/// 未知の filterId では全 0 を返す。
std::array<float, 8> resolveFilterParams(const VideoFilterInstance& inst);

} // namespace yave
