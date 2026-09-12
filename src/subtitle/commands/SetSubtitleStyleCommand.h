#pragma once

#include "../SubtitleStyle.h"

#include "../../core/commands/UndoCommandBase.h"

#include <QPointF>
#include <QVariant>
#include <QUuid>

#include <optional>

namespace yave::subtitle {

/// 字幕クリップのスタイル差分 (SubtitleStyleDiff) の単一フィールドを変更する。
///
/// プリセット + 差分方式に従い、変更はクリップ固有の差分へ記録される。
/// プリセット側は触らない。value が null の場合は差分フィールドを消して
/// プリセット値へ戻す。
enum class SubtitleStyleField
{
    FontFamily, FontPointSize, FontWeight, Italic,
    FillColor, OutlineColor, OutlineWidth,
    ShadowColor, ShadowBlur,
    BoxEnabled, BoxColor,
    HAlign, VAlign, Anchor,
    LineSpacing, LetterSpacing, MaxWidthRatio,
    RotationDeg, Scale, Opacity,
    Vertical
};

class SetSubtitleStyleCommand : public yave::UndoCommandBase
{
public:
    SetSubtitleStyleCommand(yave::Project* project, const QUuid& clipId,
                            SubtitleStyleField field, const QVariant& value);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    void apply(const QVariant& value);
    void readCurrent();

    SubtitleStyleField  field_;
    QUuid               clipId_;
    QVariant            old_;
    QVariant            new_;
};

/// フィールド名 ("fontFamily" / "fillColor" ...) から SubtitleStyleField へ。
/// 未知の名前は nullopt を返す。
std::optional<SubtitleStyleField> subtitleStyleFieldFromName(const QString& name);

} // namespace yave::subtitle