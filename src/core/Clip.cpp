#include "Clip.h"
#include "Track.h"

#include <QtGlobal>

namespace yave {

double Clip::effectiveOpacity(int64_t frame) const
{
    if (!enabled_)
        return 0.0;
    double o = opacity_;
    const int64_t rel = frame - range_.start;
    if (fadeIn_ > 0 && rel < fadeIn_)
        o *= double(rel) / double(fadeIn_);
    if (fadeOut_ > 0) {
        const int64_t toEnd = range_.end() - frame;
        if (toEnd <= fadeOut_)
            o *= double(toEnd > 0 ? toEnd : 0) / double(fadeOut_);
    }
    return qBound(0.0, o, 1.0);
}

void Clip::insertFilter(int index, VideoFilterInstance inst)
{
    if (index < 0 || index > int(filters_.size()))
        index = int(filters_.size());
    filters_.insert(filters_.begin() + index, std::move(inst));
}

void Clip::removeFilter(int index)
{
    if (index < 0 || index >= int(filters_.size()))
        return;
    filters_.erase(filters_.begin() + index);
}

void Clip::moveFilter(int from, int to)
{
    const int n = int(filters_.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    auto inst = filters_[from];
    filters_.erase(filters_.begin() + from);
    filters_.insert(filters_.begin() + to, std::move(inst));
}

void Clip::setFilterEnabled(int index, bool enabled)
{
    if (index < 0 || index >= int(filters_.size()))
        return;
    filters_[index].enabled = enabled;
}

std::vector<ResolvedFilter> Clip::resolvedFilters() const
{
    std::vector<ResolvedFilter> out;
    out.reserve(filters_.size());
    for (const auto& f : filters_) {
        if (!f.enabled)
            continue;
        out.push_back(ResolvedFilter{ f.filterId, resolveFilterParams(f) });
    }
    return out;
}

void Clip::copyBaseTo(Clip& dst) const
{
    // id は copyBaseTo では複製しない (呼び出し側で新しい id を振るか選択する)
    dst.name_               = name_;
    dst.range_              = range_;
    dst.enabled_            = enabled_;
    dst.locked_             = locked_;
    dst.opacity_            = opacity_;
    dst.blendMode_          = blendMode_;
    dst.transform_          = transform_;
    dst.crop_               = crop_;
    dst.fadeIn_             = fadeIn_;
    dst.fadeOut_            = fadeOut_;
    dst.generatedByTaskId_  = generatedByTaskId_;
    dst.filters_            = filters_;
}

} // namespace yave
