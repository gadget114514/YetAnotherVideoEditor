#pragma once

#include "SubtitleClip.h"

#include <QString>
#include <QVariantMap>

#include <vector>

namespace yave::subtitle {

/// タイトル / テロップ (3.11)。
///
/// 描画は SubtitleClip と完全に同じ経路を通る。違うのは
///   - 映像トラックにも置けること
///   - 書き出し時に字幕トラックとして分離されないこと
///   - 生成元のプリセット ID を覚えていること
/// の 3 点だけである。
///
/// 字幕と同じクラスにフラグで持たせない理由は 3.11 を参照。
class TitleClip final : public SubtitleClip
{
public:
    TitleClip();
    explicit TitleClip(const TitleClip& other);
    ~TitleClip() override;

    ClipType type() const override { return ClipType::Title; }
    std::shared_ptr<Clip> clone() const override;

    QString presetId() const { return presetId_; }
    void    setPresetId(const QString& id) { presetId_ = id; }

    /// プリセット (yave.title.center 等) の既定スタイルと既定テキストを適用する。
    void applyPreset(const QString& presetId);

private:
    QString presetId_;
};

/// 組み込みタイトルプリセットの ID。
namespace builtinTitle {
inline constexpr auto kCenter          = "yave.title.center";
inline constexpr auto kLowerThird      = "yave.title.lowerThird";
inline constexpr auto kCredits         = "yave.title.credits";
inline constexpr auto kSubtitleCaption = "yave.title.subtitleCaption";
} // namespace builtinTitle

struct TitlePresetDesc
{
    QString presetId;
    QString displayNameKey;        ///< 翻訳キー (10章)
    QString sampleTextKey;         ///< 配置直後に入る文字列の翻訳キー
    int64_t defaultDurationFrames = 180;
};

/// 組み込みタイトルプリセットの一覧。ライブラリパネル (1.7.5) が並べる。
const std::vector<TitlePresetDesc>& builtinTitlePresets();

const TitlePresetDesc* findTitlePreset(const QString& presetId);

} // namespace yave::subtitle
