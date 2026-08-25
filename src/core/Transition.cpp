#include "Transition.h"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace yave {

float Transition::progressAt(int64_t f) const
{
    if (durationFrames <= 0)
        return 1.0f;
    const double rel = double(f - startFrame()) / double(durationFrames);
    return float(std::clamp(rel, 0.0, 1.0));
}

QJsonObject Transition::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("id")]           = id.toString(QUuid::WithoutBraces);
    o[QStringLiteral("transitionId")] = transitionId;
    o[QStringLiteral("fromClipId")]   = fromClipId.isNull()
                                            ? QJsonValue(QJsonValue::Null)
                                            : QJsonValue(fromClipId.toString(QUuid::WithoutBraces));
    o[QStringLiteral("toClipId")]     = toClipId.isNull()
                                            ? QJsonValue(QJsonValue::Null)
                                            : QJsonValue(toClipId.toString(QUuid::WithoutBraces));
    o[QStringLiteral("centerFrame")]  = double(centerFrame);
    o[QStringLiteral("duration")]     = double(durationFrames);
    o[QStringLiteral("params")]       = QJsonObject::fromVariantMap(params);
    return o;
}

Transition Transition::fromJson(const QJsonObject& o)
{
    Transition t;
    t.id             = QUuid(o[QStringLiteral("id")].toString());
    if (t.id.isNull())
        t.id = QUuid::createUuid();
    t.transitionId   = o[QStringLiteral("transitionId")].toString();
    t.fromClipId     = QUuid(o[QStringLiteral("fromClipId")].toString());
    t.toClipId       = QUuid(o[QStringLiteral("toClipId")].toString());
    t.centerFrame    = int64_t(o[QStringLiteral("centerFrame")].toDouble());
    t.durationFrames = int64_t(o[QStringLiteral("duration")].toDouble());
    t.params         = o[QStringLiteral("params")].toObject().toVariantMap();
    return t;
}

const std::vector<TransitionDesc>& builtinTransitions()
{
    static const std::vector<TransitionDesc> table = {
        { QString::fromLatin1(builtinTransition::kDissolve),
          QStringLiteral("transition.dissolve.name"), {}, false },

        { QString::fromLatin1(builtinTransition::kFadeToBlack),
          QStringLiteral("transition.fadeToBlack.name"),
          { { QStringLiteral("color"), QStringLiteral("#000000") } }, true },

        { QString::fromLatin1(builtinTransition::kWipe),
          QStringLiteral("transition.wipe.name"),
          { { QStringLiteral("angle"), 0.0 }, { QStringLiteral("softness"), 0.05 } }, false },

        { QString::fromLatin1(builtinTransition::kSlide),
          QStringLiteral("transition.slide.name"),
          { { QStringLiteral("direction"), 0 } }, false },

        { QString::fromLatin1(builtinTransition::kPush),
          QStringLiteral("transition.push.name"),
          { { QStringLiteral("direction"), 0 } }, false },
    };
    return table;
}

const TransitionDesc* findTransitionDesc(const QString& transitionId)
{
    const auto& table = builtinTransitions();
    const auto it = std::find_if(table.begin(), table.end(),
                                 [&](const TransitionDesc& d) { return d.transitionId == transitionId; });
    return it != table.end() ? &*it : nullptr;
}

std::array<float, 4> resolveTransitionParams(const Transition& t)
{
    std::array<float, 4> out{};
    const auto num = [&t](const char* key, float fallback) {
        const auto it = t.params.find(QLatin1String(key));
        if (it == t.params.end())
            return fallback;
        bool ok = false;
        const float v = it->toFloat(&ok);
        return ok ? v : fallback;
    };

    if (t.transitionId == QLatin1String(builtinTransition::kWipe)) {
        // 角度は度で持ち、シェーダへはラジアンで渡す
        out[0] = float(num("angle", 0.0f) * M_PI / 180.0);
        out[1] = num("softness", 0.05f);
    }
    else if (t.transitionId == QLatin1String(builtinTransition::kSlide)
             || t.transitionId == QLatin1String(builtinTransition::kPush)) {
        out[0] = num("direction", 0.0f);
    }
    return out;
}

std::array<float, 4> resolveTransitionColor(const Transition& t)
{
    std::array<float, 4> out{ 0.0f, 0.0f, 0.0f, 1.0f };
    const auto it = t.params.find(QStringLiteral("color"));
    if (it == t.params.end())
        return out;
    const QColor c(it->toString());
    if (!c.isValid())
        return out;
    out[0] = float(c.redF());
    out[1] = float(c.greenF());
    out[2] = float(c.blueF());
    out[3] = float(c.alphaF());
    return out;
}

int transitionShaderMode(const QString& transitionId)
{
    // transition.frag の mode uniform と対応する。順番を変えないこと。
    if (transitionId == QLatin1String(builtinTransition::kFadeToBlack)) return 1;
    if (transitionId == QLatin1String(builtinTransition::kWipe))        return 2;
    if (transitionId == QLatin1String(builtinTransition::kSlide))       return 3;
    if (transitionId == QLatin1String(builtinTransition::kPush))        return 4;
    return 0;   // dissolve
}

} // namespace yave
