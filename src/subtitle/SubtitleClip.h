#pragma once

#include "../core/Clip.h"
#include "SubtitleEffectInstance.h"
#include "SubtitleStyle.h"
#include "SubtitleText.h"

#include <vector>

namespace yave::subtitle {

class SubtitleStylePresetTable;

/// 字幕区間。SRT の 1 キューがこれ 1 個に対応する。
/// タイムライン上では他のクリップと完全に同等に扱われる
/// (移動 / トリム / 分割 / 複製 / 別トラックへ移動 / リップル編集)。
class SubtitleClip : public Clip
{
public:
    SubtitleClip();
    ~SubtitleClip() override;

    /// コピーコンストラクタ。
    /// エフェクトスタックの実行時状態 (effect / prepared) は複製しない。
    /// 永続データのみコピーし、clone() で effect を張り直す。
    explicit SubtitleClip(const SubtitleClip& other);

    ClipType type() const override { return ClipType::Subtitle; }
    std::shared_ptr<Clip> clone() const override;

    // --- テキスト ---
    const SubtitleText& text() const { return text_; }
    void setText(const SubtitleText& t);

    /// 便利メソッド。リッチスパンは維持されない。
    QString plainText() const { return text_.plain(); }
    void    setPlainText(const QString& s);

    // --- スタイル ---
    /// プロジェクト共通のスタイルプリセットへの参照。
    QString stylePresetId() const { return stylePresetId_; }
    void    setStylePresetId(const QString& id);

    /// プリセットからの差分。これだけがクリップ固有の値。
    const SubtitleStyleDiff& styleOverride() const { return styleOverride_; }
    void  setStyleOverride(const SubtitleStyleDiff& d);
    void  clearStyleOverride();

    /// プリセット + 差分を解決した最終スタイル。
    SubtitleStyle resolvedStyle(const SubtitleStylePresetTable& presets) const;

    // --- エフェクトスタック ---
    /// 下から順に適用される。index 0 が最初。
    const std::vector<SubtitleEffectInstance>& effectStack() const { return effects_; }
    std::vector<SubtitleEffectInstance>&       mutableEffectStack() { return effects_; }

    void addEffect(SubtitleEffectInstance inst);
    void insertEffect(int index, SubtitleEffectInstance inst);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    void setEffectEnabled(int index, bool enabled);

    /// 未インストールプラグインを参照しているエフェクトがあるか
    bool hasMissingEffects() const;
    /// AviUtl アダプタなどブロック単位エフェクトを含むか
    bool hasBlockLevelEffect() const;

    // --- STT 由来の単語タイミング ---
    struct WordTiming
    {
        int    charStart  = 0;
        int    charLength = 0;
        double startSec   = 0.0;    ///< クリップ In からの相対秒
        double endSec     = 0.0;

        bool operator==(const WordTiming& o) const
        {
            return charStart == o.charStart && charLength == o.charLength
                && startSec == o.startSec && endSec == o.endSec;
        }
    };
    const std::vector<WordTiming>& wordTimings() const { return wordTimings_; }
    void setWordTimings(std::vector<WordTiming> w) { wordTimings_ = std::move(w); }
    bool hasWordTimings() const { return !wordTimings_.empty(); }

    // --- キャッシュ無効化 ---
    /// テキスト / スタイルが変わるとインクリメントされる。
    /// SubtitleRenderer がレイアウトとグリフアトラスの再生成判定に使う。
    uint64_t contentRevision() const { return contentRevision_; }

    // --- Clip インタフェース ---
    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

    /// 字幕はソースを持たないので常に 0 / 無制限
    int64_t sourceOffset() const override { return 0; }
    int64_t maxDuration() const override { return -1; }

private:
    void bumpContentRevision() { ++contentRevision_; }

    SubtitleText                        text_;
    QString                             stylePresetId_ = QStringLiteral("default");
    SubtitleStyleDiff                   styleOverride_;
    std::vector<SubtitleEffectInstance> effects_;
    std::vector<WordTiming>             wordTimings_;
    uint64_t                            contentRevision_ = 0;
};

} // namespace yave::subtitle
