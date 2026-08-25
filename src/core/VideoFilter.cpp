#include "VideoFilter.h"

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

namespace yave {

namespace {

/// QVariantMap -> QJsonObject。QVariantMap の値は数値 / bool / 文字列のみを想定する。
QJsonObject paramsToJson(const QVariantMap& params)
{
    return QJsonObject::fromVariantMap(params);
}

float paramFloat(const QVariantMap& p, const char* key, float fallback)
{
    const auto it = p.find(QLatin1String(key));
    if (it == p.end())
        return fallback;
    bool ok = false;
    const float v = it->toFloat(&ok);
    return ok ? v : fallback;
}

} // namespace

QJsonObject VideoFilterInstance::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("filterId")] = filterId;
    o[QStringLiteral("enabled")]  = enabled;
    o[QStringLiteral("params")]   = paramsToJson(params);
    return o;
}

VideoFilterInstance VideoFilterInstance::fromJson(const QJsonObject& o)
{
    VideoFilterInstance inst;
    inst.filterId = o[QStringLiteral("filterId")].toString();
    inst.enabled  = o[QStringLiteral("enabled")].toBool(true);
    inst.params   = o[QStringLiteral("params")].toObject().toVariantMap();
    return inst;
}

const std::vector<VideoFilterDesc>& builtinVideoFilters()
{
    static const std::vector<VideoFilterDesc> table = {
        { QString::fromLatin1(builtinFilter::kColorAdjust),
          QStringLiteral("filter.colorAdjust.name"),
          { { QStringLiteral("brightness"), 0.0 },
            { QStringLiteral("contrast"),   1.0 },
            { QStringLiteral("saturation"), 1.0 },
            { QStringLiteral("gamma"),      1.0 } } },

        { QString::fromLatin1(builtinFilter::kBlur),
          QStringLiteral("filter.blur.name"),
          { { QStringLiteral("radius"),    8.0 },
            { QStringLiteral("direction"), 0 } } },

        { QString::fromLatin1(builtinFilter::kMono),
          QStringLiteral("filter.mono.name"),
          { { QStringLiteral("amount"), 1.0 } } },

        { QString::fromLatin1(builtinFilter::kSepia),
          QStringLiteral("filter.sepia.name"),
          { { QStringLiteral("amount"), 1.0 } } },
    };
    return table;
}

const VideoFilterDesc* findVideoFilterDesc(const QString& filterId)
{
    const auto& table = builtinVideoFilters();
    const auto it = std::find_if(table.begin(), table.end(),
                                 [&](const VideoFilterDesc& d) { return d.filterId == filterId; });
    return it != table.end() ? &*it : nullptr;
}

std::array<float, 8> resolveFilterParams(const VideoFilterInstance& inst)
{
    // スロットの意味は 3.9.2 の表で固定されている。ここと filter_*.frag の
    // 読み出し順は必ず一致させること。
    std::array<float, 8> out{};

    if (inst.filterId == QLatin1String(builtinFilter::kColorAdjust)) {
        out[0] = paramFloat(inst.params, "brightness", 0.0f);
        out[1] = paramFloat(inst.params, "contrast",   1.0f);
        out[2] = paramFloat(inst.params, "saturation", 1.0f);
        out[3] = paramFloat(inst.params, "gamma",      1.0f);
    }
    else if (inst.filterId == QLatin1String(builtinFilter::kBlur)) {
        out[0] = paramFloat(inst.params, "radius",    8.0f);
        out[1] = paramFloat(inst.params, "direction", 0.0f);
    }
    else if (inst.filterId == QLatin1String(builtinFilter::kMono)
             || inst.filterId == QLatin1String(builtinFilter::kSepia)) {
        out[0] = paramFloat(inst.params, "amount", 1.0f);
    }
    return out;
}

} // namespace yave
