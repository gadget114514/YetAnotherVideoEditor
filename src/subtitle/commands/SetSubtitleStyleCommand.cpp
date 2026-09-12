#include "SetSubtitleStyleCommand.h"

#include "../SubtitleClip.h"

#include "../../core/Project.h"
#include "../../core/Timeline.h"
#include "../../core/Track.h"

#include <QColor>
#include <QHash>
#include <QObject>

namespace yave::subtitle {

namespace {

SubtitleClip* findSubtitleClip(yave::Project* project, const QUuid& clipId)
{
    yave::Timeline* tl = project ? project->timeline() : nullptr;
    if (!tl)
        return nullptr;
    auto clip = tl->findClip(clipId);
    if (!clip)
        return nullptr;
    if (clip->type() != yave::ClipType::Subtitle && clip->type() != yave::ClipType::Title)
        return nullptr;
    return static_cast<SubtitleClip*>(clip.get());
}

} // anonymous namespace

std::optional<SubtitleStyleField> subtitleStyleFieldFromName(const QString& name)
{
    static const QHash<QString, SubtitleStyleField> table = {
        { QStringLiteral("fontFamily"),    SubtitleStyleField::FontFamily },
        { QStringLiteral("fontPointSize"), SubtitleStyleField::FontPointSize },
        { QStringLiteral("fontWeight"),    SubtitleStyleField::FontWeight },
        { QStringLiteral("italic"),        SubtitleStyleField::Italic },
        { QStringLiteral("fillColor"),     SubtitleStyleField::FillColor },
        { QStringLiteral("outlineColor"),  SubtitleStyleField::OutlineColor },
        { QStringLiteral("outlineWidth"),  SubtitleStyleField::OutlineWidth },
        { QStringLiteral("shadowColor"),   SubtitleStyleField::ShadowColor },
        { QStringLiteral("shadowBlur"),    SubtitleStyleField::ShadowBlur },
        { QStringLiteral("boxEnabled"),    SubtitleStyleField::BoxEnabled },
        { QStringLiteral("boxColor"),      SubtitleStyleField::BoxColor },
        { QStringLiteral("hAlign"),        SubtitleStyleField::HAlign },
        { QStringLiteral("vAlign"),        SubtitleStyleField::VAlign },
        { QStringLiteral("anchor"),        SubtitleStyleField::Anchor },
        { QStringLiteral("lineSpacing"),   SubtitleStyleField::LineSpacing },
        { QStringLiteral("letterSpacing"), SubtitleStyleField::LetterSpacing },
        { QStringLiteral("maxWidthRatio"), SubtitleStyleField::MaxWidthRatio },
        { QStringLiteral("rotationDeg"),   SubtitleStyleField::RotationDeg },
        { QStringLiteral("scale"),         SubtitleStyleField::Scale },
        { QStringLiteral("opacity"),       SubtitleStyleField::Opacity },
        { QStringLiteral("vertical"),      SubtitleStyleField::Vertical },
    };
    const auto it = table.constFind(name);
    if (it != table.cend())
        return *it;
    return std::nullopt;
}

namespace {

/// 現在の差分フィールドを QVariant として読む (未設定は null)。
QVariant diffFieldValue(const SubtitleStyleDiff& d, SubtitleStyleField field)
{
    switch (field) {
    case SubtitleStyleField::FontFamily:    return d.fontFamily ? QVariant(*d.fontFamily) : QVariant();
    case SubtitleStyleField::FontPointSize: return d.fontPointSize ? QVariant(*d.fontPointSize) : QVariant();
    case SubtitleStyleField::FontWeight:    return d.fontWeight ? QVariant(*d.fontWeight) : QVariant();
    case SubtitleStyleField::Italic:        return d.italic ? QVariant(*d.italic) : QVariant();
    case SubtitleStyleField::FillColor:     return d.fillColor ? QVariant(*d.fillColor) : QVariant();
    case SubtitleStyleField::OutlineColor:  return d.outlineColor ? QVariant(*d.outlineColor) : QVariant();
    case SubtitleStyleField::OutlineWidth:  return d.outlineWidth ? QVariant(*d.outlineWidth) : QVariant();
    case SubtitleStyleField::ShadowColor:   return d.shadowColor ? QVariant(*d.shadowColor) : QVariant();
    case SubtitleStyleField::ShadowBlur:    return d.shadowBlur ? QVariant(*d.shadowBlur) : QVariant();
    case SubtitleStyleField::BoxEnabled:    return d.boxEnabled ? QVariant(*d.boxEnabled) : QVariant();
    case SubtitleStyleField::BoxColor:      return d.boxColor ? QVariant(*d.boxColor) : QVariant();
    case SubtitleStyleField::HAlign:        return d.hAlign ? QVariant(*d.hAlign) : QVariant();
    case SubtitleStyleField::VAlign:        return d.vAlign ? QVariant(*d.vAlign) : QVariant();
    case SubtitleStyleField::Anchor:        return d.anchor ? QVariant(*d.anchor) : QVariant();
    case SubtitleStyleField::LineSpacing:   return d.lineSpacing ? QVariant(*d.lineSpacing) : QVariant();
    case SubtitleStyleField::LetterSpacing: return d.letterSpacing ? QVariant(*d.letterSpacing) : QVariant();
    case SubtitleStyleField::MaxWidthRatio: return d.maxWidthRatio ? QVariant(*d.maxWidthRatio) : QVariant();
    case SubtitleStyleField::RotationDeg:   return d.rotationDeg ? QVariant(*d.rotationDeg) : QVariant();
    case SubtitleStyleField::Scale:         return d.scale ? QVariant(*d.scale) : QVariant();
    case SubtitleStyleField::Opacity:       return d.opacity ? QVariant(*d.opacity) : QVariant();
    case SubtitleStyleField::Vertical:      return d.vertical ? QVariant(*d.vertical) : QVariant();
    }
    return {};
}

void setDiffField(SubtitleStyleDiff& d, SubtitleStyleField field, const QVariant& v)
{
    const bool set = v.isValid() && !v.isNull();
    auto toColor = [](const QVariant& val) -> QColor {
        if (val.canConvert<QColor>())
            return val.value<QColor>();
        return QColor(val.toString());
    };
    switch (field) {
    case SubtitleStyleField::FontFamily:    d.fontFamily    = set ? v.toString() : std::optional<QString>{}; break;
    case SubtitleStyleField::FontPointSize: d.fontPointSize = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::FontWeight:    d.fontWeight    = set ? v.toInt() : std::optional<int>{}; break;
    case SubtitleStyleField::Italic:        d.italic        = set ? v.toBool() : std::optional<bool>{}; break;
    case SubtitleStyleField::FillColor:     d.fillColor     = set ? toColor(v) : std::optional<QColor>{}; break;
    case SubtitleStyleField::OutlineColor:  d.outlineColor  = set ? toColor(v) : std::optional<QColor>{}; break;
    case SubtitleStyleField::OutlineWidth:  d.outlineWidth  = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::ShadowColor:   d.shadowColor   = set ? toColor(v) : std::optional<QColor>{}; break;
    case SubtitleStyleField::ShadowBlur:    d.shadowBlur    = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::BoxEnabled:    d.boxEnabled    = set ? v.toBool() : std::optional<bool>{}; break;
    case SubtitleStyleField::BoxColor:      d.boxColor      = set ? toColor(v) : std::optional<QColor>{}; break;
    case SubtitleStyleField::HAlign:        d.hAlign        = set ? v.toInt() : std::optional<int>{}; break;
    case SubtitleStyleField::VAlign:        d.vAlign        = set ? v.toInt() : std::optional<int>{}; break;
    case SubtitleStyleField::Anchor:        d.anchor        = set ? v.toPointF() : std::optional<QPointF>{}; break;
    case SubtitleStyleField::LineSpacing:   d.lineSpacing   = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::LetterSpacing: d.letterSpacing = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::MaxWidthRatio: d.maxWidthRatio = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::RotationDeg:   d.rotationDeg   = set ? v.toDouble() : std::optional<double>{}; break;
    case SubtitleStyleField::Scale:         d.scale         = set ? v.toPointF() : std::optional<QPointF>{}; break;
    case SubtitleStyleField::Opacity:       d.opacity       = set ? qBound(0.0, v.toDouble(), 1.0) : std::optional<double>{}; break;
    case SubtitleStyleField::Vertical:      d.vertical      = set ? v.toBool() : std::optional<bool>{}; break;
    }
}

} // anonymous namespace

SetSubtitleStyleCommand::SetSubtitleStyleCommand(yave::Project* project,
                                                 const QUuid& clipId,
                                                 SubtitleStyleField field,
                                                 const QVariant& value)
    : yave::UndoCommandBase(project, QObject::tr("Change subtitle style"))
    , field_(field)
    , clipId_(clipId)
    , new_(value)
{
    readCurrent();
}

void SetSubtitleStyleCommand::readCurrent()
{
    if (const SubtitleClip* clip = findSubtitleClip(project(), clipId_))
        old_ = diffFieldValue(clip->styleOverride(), field_);
}

void SetSubtitleStyleCommand::apply(const QVariant& value)
{
    SubtitleClip* clip = findSubtitleClip(project(), clipId_);
    if (!clip)
        return;

    SubtitleStyleDiff d = clip->styleOverride();
    setDiffField(d, field_, value);
    clip->setStyleOverride(d);

    yave::Timeline* tl = project() ? project()->timeline() : nullptr;
    if (tl) {
        yave::Track* owner = nullptr;
        (void)tl->findClip(clipId_, &owner);
        if (owner)
            emit tl->clipChanged(owner->id(), clipId_);
    }
}

void SetSubtitleStyleCommand::doRedo() { apply(new_); }
void SetSubtitleStyleCommand::doUndo() { apply(old_); }

} // namespace yave::subtitle