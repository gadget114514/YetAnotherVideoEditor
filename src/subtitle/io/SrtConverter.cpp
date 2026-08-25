#include "SrtParser.h"
#include "../SubtitleClip.h"
#include "../SubtitleText.h"

#include <QFile>
#include <QObject>

#include <algorithm>

namespace yave::subtitle {

std::vector<std::shared_ptr<SubtitleClip>> convertCuesToClips(
    const SrtParseResult& parsed,
    const Rational& timebase,
    const QString& stylePresetId,
    QStringList* warningsOut)
{
    std::vector<std::shared_ptr<SubtitleClip>> clips;
    clips.reserve(parsed.cues.size());

    int64_t prevEnd = std::numeric_limits<int64_t>::min();

    for (const SrtCue& cue : parsed.cues) {
        // 秒 -> フレーム。開始は Floor、終了は Ceil。
        const int64_t startF = secondsToFrames(cue.startSeconds, timebase, RoundMode::Floor);
        const int64_t endF   = secondsToFrames(cue.endSeconds,   timebase, RoundMode::Ceil);

        if (endF <= startF) {
            if (warningsOut)
                warningsOut->append(QObject::tr("Cue #%1 has zero or negative duration; skipped.")
                                        .arg(cue.index));
            continue;
        }
        if (startF < prevEnd && warningsOut) {
            // 重なりは自動修正しない。ユーザーに提示して判断させる。
            warningsOut->append(QObject::tr("Cue #%1 overlaps the previous cue.").arg(cue.index));
        }
        prevEnd = endF;

        auto clip = std::make_shared<SubtitleClip>();
        clip->setId(QUuid::createUuid());
        clip->setRange({startF, endF - startF});
        clip->setText(SubtitleText::fromSrtMarkup(cue.rawText));
        clip->setStylePresetId(stylePresetId);
        clips.push_back(std::move(clip));
    }
    return clips;
}

} // namespace yave::subtitle
