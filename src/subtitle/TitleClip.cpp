#include "TitleClip.h"

#include "SubtitleEffectFactoryBridge.h"

#include <QCoreApplication>

#include <algorithm>

namespace yave::subtitle {

TitleClip::TitleClip() = default;
TitleClip::~TitleClip() = default;

TitleClip::TitleClip(const TitleClip& other)
    : SubtitleClip(other)
    , presetId_(other.presetId_)
{
}

std::shared_ptr<Clip> TitleClip::clone() const
{
    auto c = std::make_shared<TitleClip>(*this);
    c->setId(QUuid::createUuid());

    // エフェクトは新規生成する (prepare() の前計算結果はインスタンス固有)。
    // SubtitleClip::clone() と同じ理由。
    auto& stack = c->mutableEffectStack();
    for (auto& inst : stack)
        inst.effect = createEffectViaFactory(inst.effectId);

    return c;
}

const std::vector<TitlePresetDesc>& builtinTitlePresets()
{
    static const std::vector<TitlePresetDesc> table = {
        { QString::fromLatin1(builtinTitle::kCenter),
          QStringLiteral("title.center.name"),   QStringLiteral("title.center.sample"),   180 },
        { QString::fromLatin1(builtinTitle::kLowerThird),
          QStringLiteral("title.lowerThird.name"), QStringLiteral("title.lowerThird.sample"), 180 },
        { QString::fromLatin1(builtinTitle::kCredits),
          QStringLiteral("title.credits.name"),  QStringLiteral("title.credits.sample"),  600 },
        { QString::fromLatin1(builtinTitle::kSubtitleCaption),
          QStringLiteral("title.subtitleCaption.name"),
          QStringLiteral("title.subtitleCaption.sample"), 120 },
    };
    return table;
}

const TitlePresetDesc* findTitlePreset(const QString& presetId)
{
    const auto& table = builtinTitlePresets();
    const auto it = std::find_if(table.begin(), table.end(),
                                 [&](const TitlePresetDesc& d) { return d.presetId == presetId; });
    return it != table.end() ? &*it : nullptr;
}

void TitleClip::applyPreset(const QString& presetId)
{
    presetId_ = presetId;

    SubtitleStyleDiff diff;

    if (presetId == QLatin1String(builtinTitle::kCenter)) {
        diff.fontPointSize = 96.0;
        diff.fontWeight    = 900;
        diff.hAlign        = int(HAlign::Center);
        diff.vAlign        = int(VAlign::Middle);
        diff.anchor        = QPointF(0.5, 0.5);
        setName(QCoreApplication::translate("TitleClip", "Title"));
    }
    else if (presetId == QLatin1String(builtinTitle::kLowerThird)) {
        diff.fontPointSize = 56.0;
        diff.hAlign        = int(HAlign::Left);
        diff.vAlign        = int(VAlign::Bottom);
        diff.anchor        = QPointF(0.08, 0.78);
        diff.boxEnabled    = true;
        setName(QCoreApplication::translate("TitleClip", "Lower Third"));
    }
    else if (presetId == QLatin1String(builtinTitle::kCredits)) {
        diff.fontPointSize = 44.0;
        diff.hAlign        = int(HAlign::Center);
        diff.vAlign        = int(VAlign::Middle);
        diff.anchor        = QPointF(0.5, 0.5);
        setName(QCoreApplication::translate("TitleClip", "Credits"));
    }
    else {   // kSubtitleCaption / 未知のプリセット
        diff.fontPointSize = 48.0;
        diff.hAlign        = int(HAlign::Center);
        diff.vAlign        = int(VAlign::Bottom);
        diff.anchor        = QPointF(0.5, 0.92);
        setName(QCoreApplication::translate("TitleClip", "Caption"));
    }

    setStyleOverride(diff);
}

} // namespace yave::subtitle
