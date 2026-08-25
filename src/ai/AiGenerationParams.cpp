#include "AiGenerationParams.h"

#include "../util/Hash.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

namespace yave::ai {

// ===========================================================================
//  ImageReference / VideoReference
// ===========================================================================

QJsonObject ImageReference::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("source")] =
        (source == Source::FilePath) ? QStringLiteral("filePath")
                                     : QStringLiteral("timelineFrame");
    if (source == Source::FilePath) {
        o[QStringLiteral("filePath")] = filePath;         ///< プロジェクト相対
    } else {
        o[QStringLiteral("sourceTrackId")] = sourceTrackId.toString(QUuid::WithoutBraces);
        o[QStringLiteral("sourceFrame")]   = double(sourceFrame);
    }
    o[QStringLiteral("strength")] = strength;
    return o;
}

ImageReference ImageReference::fromJson(const QJsonObject& o)
{
    ImageReference r;
    r.source = (o[QStringLiteral("source")].toString() == QLatin1String("timelineFrame"))
                 ? Source::TimelineFrame : Source::FilePath;
    r.filePath      = o[QStringLiteral("filePath")].toString();
    r.sourceTrackId = QUuid(o[QStringLiteral("sourceTrackId")].toString());
    r.sourceFrame   = int64_t(o[QStringLiteral("sourceFrame")].toDouble(0));
    r.strength      = o[QStringLiteral("strength")].toDouble(1.0);
    return r;
}

QJsonObject VideoReference::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("sourceTrackId")] = sourceTrackId.toString(QUuid::WithoutBraces);
    o[QStringLiteral("start")]  = double(sourceRange.start);
    o[QStringLiteral("duration")] = double(sourceRange.duration);
    o[QStringLiteral("denoiseStrength")] = denoiseStrength;
    return o;
}

VideoReference VideoReference::fromJson(const QJsonObject& o)
{
    VideoReference r;
    r.sourceTrackId = QUuid(o[QStringLiteral("sourceTrackId")].toString());
    r.sourceRange.start    = int64_t(o[QStringLiteral("start")].toDouble(0));
    r.sourceRange.duration = int64_t(o[QStringLiteral("duration")].toDouble(0));
    r.denoiseStrength      = o[QStringLiteral("denoiseStrength")].toDouble(0.6);
    return r;
}

// ===========================================================================
//  シリアライズ
// ===========================================================================

namespace {

QString uuidKey(const QUuid& u)
{
    return u.isNull() ? QString() : u.toString(QUuid::WithoutBraces);
}

QUuid uuidFrom(const QJsonValue& v)
{
    const QString s = v.toString();
    return s.isEmpty() ? QUuid() : QUuid(s);
}

/// seed は 64bit を取りうるので文字列で保存する
/// (QJsonValue は double 保持のため 2^53 を超えると精度が落ちる)
int64_t int64FromJson(const QJsonValue& v, int64_t fallback)
{
    if (v.isString()) {
        bool ok = false;
        const auto r = v.toString().toLongLong(&ok);
        return ok ? r : fallback;
    }
    if (v.isDouble())
        return int64_t(v.toDouble());
    return fallback;
}

} // anonymous namespace

QJsonObject AiGenerationParams::toJson() const
{
    QJsonObject o;

    // ---- 共通 ----
    o[QStringLiteral("kind")]          = int(kind);
    o[QStringLiteral("targetTrackId")] = uuidKey(targetTrackId);
    o[QStringLiteral("rangeStart")]    = double(range.start);
    o[QStringLiteral("rangeDuration")] = double(range.duration);
    o[QStringLiteral("modelId")]       = modelId;
    if (!providerId.isEmpty())
        o[QStringLiteral("providerId")] = providerId;
    o[QStringLiteral("prompt")]         = prompt;
    if (!negativePrompt.isEmpty())
        o[QStringLiteral("negativePrompt")] = negativePrompt;
    // seed は 64bit を取りうるので文字列で保存する
    o[QStringLiteral("seed")] = QString::number(seed);
    o[QStringLiteral("steps")]          = steps;
    o[QStringLiteral("guidanceScale")]  = guidanceScale;

    switch (kind) {
    case GenerationKind::Video:
    case GenerationKind::Image: {
        o[QStringLiteral("videoMode")] = int(videoMode);
        o[QStringLiteral("i2vRefMode")] = int(i2vRefMode);
        if (startReference)
            o[QStringLiteral("startReference")] = startReference->toJson();
        if (endReference)
            o[QStringLiteral("endReference")] = endReference->toJson();
        if (videoReference)
            o[QStringLiteral("videoReference")] = videoReference->toJson();
        o[QStringLiteral("outWidth")]  = outputResolution.width();
        o[QStringLiteral("outHeight")] = outputResolution.height();
        o[QStringLiteral("outFpsNum")] = double(outputFrameRate.num);
        o[QStringLiteral("outFpsDen")] = double(outputFrameRate.den);
        o[QStringLiteral("loopSeamless")] = loopSeamless;
        break;
    }
    case GenerationKind::Audio:
        o[QStringLiteral("audioMode")]    = int(audioMode);
        o[QStringLiteral("voiceId")]      = voiceId;
        o[QStringLiteral("speakingRate")] = speakingRate;
        o[QStringLiteral("pitch")]        = pitch;
        if (!referenceAudioPath.isEmpty())
            o[QStringLiteral("referenceAudioPath")] = referenceAudioPath;
        o[QStringLiteral("audioSampleRate")] = audioSampleRate;
        o[QStringLiteral("audioChannels")]   = audioChannels;
        o[QStringLiteral("targetLufs")]      = targetLufs;
        break;
    case GenerationKind::Subtitle:
        o[QStringLiteral("subtitleMode")]     = int(subtitleMode);
        o[QStringLiteral("sourceAudioTrackId")] = uuidKey(sourceAudioTrackId);
        o[QStringLiteral("language")]         = language;
        if (!targetLanguage.isEmpty())
            o[QStringLiteral("targetLanguage")] = targetLanguage;
        o[QStringLiteral("wordLevelTimestamps")] = wordLevelTimestamps;
        o[QStringLiteral("subtitleStylePresetId")] = subtitleStylePresetId;
        if (maxCharsPerCue > 0)
            o[QStringLiteral("maxCharsPerCue")] = maxCharsPerCue;
        if (minCueDurationFrames > 0)
            o[QStringLiteral("minCueDurationFrames")] = double(minCueDurationFrames);
        break;
    case GenerationKind::Mask:
        o[QStringLiteral("maskSourceTrackId")]     = uuidKey(maskSourceTrackId);
        o[QStringLiteral("maskTargetDescription")] = maskTargetDescription;
        if (!maskHintPoints.empty()) {
            QJsonArray hints;
            for (const QPointF& pt : maskHintPoints) {
                QJsonObject h;
                h[QStringLiteral("x")] = pt.x();
                h[QStringLiteral("y")] = pt.y();
                hints.append(h);
            }
            o[QStringLiteral("maskHintPoints")] = hints;
        }
        o[QStringLiteral("maskTrackAcrossFrames")] = maskTrackAcrossFrames;
        o[QStringLiteral("maskFeather")]           = maskFeather;
        break;
    case GenerationKind::EffectMetadata:
        break;
    }

    o[QStringLiteral("replaceExistingClips")] = replaceExistingClips;
    o[QStringLiteral("createNewTrack")]       = createNewTrack;

    if (!extraParams.isEmpty())
        o[QStringLiteral("extraParams")] = extraParams;

    return o;
}

AiGenerationParams AiGenerationParams::fromJson(const QJsonObject& o)
{
    AiGenerationParams p;

    p.kind          = GenerationKind(o[QStringLiteral("kind")].toInt(int(GenerationKind::Video)));
    p.targetTrackId = uuidFrom(o[QStringLiteral("targetTrackId")]);
    p.range.start    = int64_t(o[QStringLiteral("rangeStart")].toDouble(0));
    p.range.duration = int64_t(o[QStringLiteral("rangeDuration")].toDouble(0));
    p.modelId        = o[QStringLiteral("modelId")].toString();
    p.providerId     = o[QStringLiteral("providerId")].toString();
    p.prompt         = o[QStringLiteral("prompt")].toString();
    p.negativePrompt = o[QStringLiteral("negativePrompt")].toString();
    p.seed           = int64FromJson(o[QStringLiteral("seed")], -1);
    p.steps          = o[QStringLiteral("steps")].toInt(30);
    p.guidanceScale  = o[QStringLiteral("guidanceScale")].toDouble(7.5);

    switch (p.kind) {
    case GenerationKind::Video:
    case GenerationKind::Image: {
        p.videoMode  = VideoGenMode(o[QStringLiteral("videoMode")].toInt(0));
        p.i2vRefMode = I2VReferenceMode(o[QStringLiteral("i2vRefMode")].toInt(0));
        if (o[QStringLiteral("startReference")].isObject())
            p.startReference = ImageReference::fromJson(
                o[QStringLiteral("startReference")].toObject());
        if (o[QStringLiteral("endReference")].isObject())
            p.endReference = ImageReference::fromJson(
                o[QStringLiteral("endReference")].toObject());
        if (o[QStringLiteral("videoReference")].isObject())
            p.videoReference = VideoReference::fromJson(
                o[QStringLiteral("videoReference")].toObject());
        p.outputResolution = QSize(o[QStringLiteral("outWidth")].toInt(1280),
                                   o[QStringLiteral("outHeight")].toInt(720));
        p.outputFrameRate = Rational(int64_t(o[QStringLiteral("outFpsNum")].toDouble(1)),
                                     int64_t(o[QStringLiteral("outFpsDen")].toDouble(30)));
        p.loopSeamless = o[QStringLiteral("loopSeamless")].toBool(false);
        break;
    }
    case GenerationKind::Audio:
        p.audioMode    = AudioGenMode(o[QStringLiteral("audioMode")].toInt(0));
        p.voiceId      = o[QStringLiteral("voiceId")].toString();
        p.speakingRate = o[QStringLiteral("speakingRate")].toDouble(1.0);
        p.pitch        = o[QStringLiteral("pitch")].toDouble(0.0);
        p.referenceAudioPath = o[QStringLiteral("referenceAudioPath")].toString();
        p.audioSampleRate    = o[QStringLiteral("audioSampleRate")].toInt(48000);
        p.audioChannels      = o[QStringLiteral("audioChannels")].toInt(2);
        p.targetLufs         = o[QStringLiteral("targetLufs")].toDouble(-16.0);
        break;
    case GenerationKind::Subtitle:
        p.subtitleMode = SubtitleGenMode(o[QStringLiteral("subtitleMode")].toInt(0));
        p.sourceAudioTrackId = uuidFrom(o[QStringLiteral("sourceAudioTrackId")]);
        p.language           = o[QStringLiteral("language")].toString();
        p.targetLanguage     = o[QStringLiteral("targetLanguage")].toString();
        p.wordLevelTimestamps = o[QStringLiteral("wordLevelTimestamps")].toBool(true);
        p.subtitleStylePresetId =
            o[QStringLiteral("subtitleStylePresetId")].toString(QStringLiteral("default"));
        p.maxCharsPerCue      = o[QStringLiteral("maxCharsPerCue")].toInt(0);
        p.minCueDurationFrames =
            int64_t(o[QStringLiteral("minCueDurationFrames")].toDouble(0));
        break;
    case GenerationKind::Mask:
        p.maskSourceTrackId     = uuidFrom(o[QStringLiteral("maskSourceTrackId")]);
        p.maskTargetDescription = o[QStringLiteral("maskTargetDescription")].toString();
        for (const QJsonValue& v : o[QStringLiteral("maskHintPoints")].toArray()) {
            const QJsonObject h = v.toObject();
            p.maskHintPoints.emplace_back(h[QStringLiteral("x")].toDouble(),
                                          h[QStringLiteral("y")].toDouble());
        }
        p.maskTrackAcrossFrames = o[QStringLiteral("maskTrackAcrossFrames")].toBool(true);
        p.maskFeather           = o[QStringLiteral("maskFeather")].toBool(true);
        break;
    case GenerationKind::EffectMetadata:
        break;
    }

    p.replaceExistingClips = o[QStringLiteral("replaceExistingClips")].toBool(false);
    p.createNewTrack       = o[QStringLiteral("createNewTrack")].toBool(false);
    p.extraParams          = o[QStringLiteral("extraParams")].toObject();

    return p;
}

// ===========================================================================
//  検証 / ハッシュ
// ===========================================================================

AiGenerationParams::ValidationResult AiGenerationParams::validate() const
{
    ValidationResult r;

    if (range.isEmpty()) {
        r.ok = false;
        r.errorKey = QStringLiteral("error.ai.emptyRange");
        return r;
    }

    switch (kind) {
    case GenerationKind::Video:
    case GenerationKind::Image:
        if (videoMode == VideoGenMode::ImageToVideo && !startReference) {
            r.ok = false;
            r.errorKey = QStringLiteral("error.ai.missingStartRef");
        }
        break;
    case GenerationKind::Audio:
        if (audioMode == AudioGenMode::Narration && prompt.isEmpty()
            && referenceAudioPath.isEmpty()) {
            r.ok = false;
            r.errorKey = QStringLiteral("error.ai.missingNarrationInput");
        }
        break;
    case GenerationKind::Subtitle:
        if (subtitleMode == SubtitleGenMode::SpeechToText
            && sourceAudioTrackId.isNull()) {
            r.ok = false;
            r.errorKey = QStringLiteral("error.ai.missingSourceAudio");
        }
        break;
    case GenerationKind::Mask:
        if (maskTargetDescription.isEmpty() && maskHintPoints.empty()) {
            r.ok = false;
            r.errorKey = QStringLiteral("error.ai.missingMaskTarget");
        }
        break;
    case GenerationKind::EffectMetadata:
        break;
    }

    return r;
}

QByteArray AiGenerationParams::contentHash() const
{
    // 配置先を除いた「内容」のみでハッシュを計算する。
    // 同じ内容の再生成はキャッシュヒットさせるため。
    quint64 h = 0xcbf29ce484222325ULL;
    hashCombine(h, quint64(kind));
    hashCombine(h, hashString(modelId));
    hashCombine(h, hashString(prompt));
    hashCombine(h, hashString(negativePrompt));
    hashCombine(h, quint64(seed));
    hashCombine(h, quint64(steps));
    hashCombine(h, quint64(guidanceScale * 1000.0));

    QString payload = QString::number(range.start) + QLatin1Char('|')
                    + QString::number(range.duration) + QLatin1Char('|')
                    + modelId + QLatin1Char('|') + prompt + QLatin1Char('|');
    payload += QString::fromUtf8(QJsonDocument(extraParams).toJson(QJsonDocument::Compact));

    QCryptographicHash sha(QCryptographicHash::Sha256);
    sha.addData(payload.toUtf8());
    return sha.result().toHex();
}

} // namespace yave::ai
