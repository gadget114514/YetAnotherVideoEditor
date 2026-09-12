#pragma once

#include "../SubtitleText.h"

#include "../../core/commands/UndoCommandBase.h"

#include <QUuid>

namespace yave::subtitle {

/// 字幕 / タイトルクリップのテキストを Undo 可能に編集する。
///
/// 連続入力は 1 コマンドにマージされる (IdEditSubtitleText)。
/// リッチスパン (太字・色など) は維持しつつ、入力されたプレーン文字列の
/// テキスト部分のみを置き換える。エフェクトの実行時状態は触らない。
class EditSubtitleTextCommand : public yave::UndoCommandBase
{
public:
    EditSubtitleTextCommand(yave::Project* project, const QUuid& clipId,
                            const QString& newPlainText);

    int  id() const override { return yave::UndoCommandBase::IdEditSubtitleText; }
    bool mergeWith(const QUndoCommand* other) override;

protected:
    void doRedo() override;
    void doUndo() override;

private:
    void apply(const QString& plainText);

    QUuid   clipId_;
    QString old_;
    QString new_;
};

} // namespace yave::subtitle