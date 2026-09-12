#include "EditSubtitleTextCommand.h"

#include "../SubtitleClip.h"

#include "../../core/Project.h"
#include "../../core/Timeline.h"
#include "../../core/Track.h"

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

EditSubtitleTextCommand::EditSubtitleTextCommand(yave::Project* project,
                                                 const QUuid& clipId,
                                                 const QString& newPlainText)
    : yave::UndoCommandBase(project, QObject::tr("Edit subtitle text"))
    , clipId_(clipId)
    , new_(newPlainText)
{
    if (const SubtitleClip* clip = findSubtitleClip(project, clipId))
        old_ = clip->plainText();
}

bool EditSubtitleTextCommand::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id())
        return false;
    const auto* cmd = static_cast<const EditSubtitleTextCommand*>(other);
    if (cmd->clipId_ != clipId_)
        return false;
    // 間に入っている Undo コマンドの text を最新のものに書き換える
    // (クリップ名の表示や Undo メニューが入力中の文字列を示すように)。
    new_ = cmd->new_;
    setText(QObject::tr("Edit subtitle text"));
    return true;
}

void EditSubtitleTextCommand::apply(const QString& plainText)
{
    SubtitleClip* clip = findSubtitleClip(project(), clipId_);
    if (!clip)
        return;

    // リッチスパンは文字位置ベースのため、プレーン文字列を保ったまま
    // テキスト本体だけを置き換える。スパン範囲外へ伸びた分は normalize が落とす。
    SubtitleText text = clip->text();
    text.setPlain(plainText);
    clip->setText(text);

    yave::Timeline* tl = project() ? project()->timeline() : nullptr;
    if (tl) {
        yave::Track* owner = nullptr;
        (void)tl->findClip(clipId_, &owner);
        if (owner)
            emit tl->clipChanged(owner->id(), clipId_);
    }
}

void EditSubtitleTextCommand::doRedo() { apply(new_); }
void EditSubtitleTextCommand::doUndo() { apply(old_); }

} // namespace yave::subtitle