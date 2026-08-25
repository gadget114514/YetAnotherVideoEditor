#pragma once

#include "ParameterSchema.h"
#include "YaveSdkVersion.h"

#include <QMatrix4x4>
#include <QColor>
#include <QRectF>
#include <QSize>
#include <QString>

#include <vector>

class QWidget;

namespace yave::sdk {

/// グリフ 1 個分の、フレームごとに変化する状態。
/// エフェクトはこれだけを書き換える。
struct GlyphTransform
{
    QMatrix4x4 transform;                  ///< グリフ中心を原点とする変換
    QColor     color      = Qt::white;     ///< 乗算色
    float      opacity    = 1.0f;
    bool       visible    = true;
    float      blurRadius = 0.0f;          ///< px
};

/// レイアウト済みグリフの静的情報 (読み取り専用)
struct GlyphInfo
{
    int    charIndex    = 0;
    int    wordIndex    = 0;
    int    lineIndex    = 0;
    QRectF layoutRect;
    QRectF atlasUv;
    QColor baseColor;
    bool   isWhitespace = false;
};

/// レイアウト結果全体。
/// 物理的には 1 枚のブロック画像(グリフアトラス)で、各グリフの矩形が atlasUv で指される。
struct SubtitleGlyphRun
{
    std::vector<GlyphInfo> glyphs;
    QSizeF                 blockSize;
    int                    lineCount = 0;
    QSize                  atlasSize;
    quint64                cacheKey  = 0;
};

/// apply() に渡される I/O 構造体
struct SubtitleEffectFrame
{
    // ---- 読み取り専用 ----
    const SubtitleGlyphRun* run = nullptr;
    QSize   canvasSize;
    qint64  clipStartFrame = 0;
    qint64  clipDuration   = 0;
    qint64  currentFrame   = 0;
    double  fps            = 60.0;

    /// STT 由来の単語タイミング。無い場合は null。
    struct WordTiming { int charStart; int charLength; double startSec; double endSec; };
    const std::vector<WordTiming>* wordTimings = nullptr;

    // ---- 読み書き (エフェクトはここだけを変更する) ----
    std::vector<GlyphTransform>* glyphs         = nullptr;   ///< run->glyphs と同要素数
    QMatrix4x4*                  blockTransform = nullptr;
    float*                       blockOpacity   = nullptr;
};

/// 時間情報
struct SubtitleTimeInfo
{
    double progress        = 0.0;   ///< 0.0 (In) .. 1.0 (Out)
    double secondsFromIn   = 0.0;
    double secondsToOut    = 0.0;
    double clipDurationSec = 0.0;
};

/// 字幕エフェクトのインタフェース。
/// 組み込みエフェクトも外部プラグインもこれを実装する。
///
/// 契約:
///   - apply() はグリフアトラスの再生成を要求してはならない
///   - apply() は frame.run を変更してはならない
///   - apply() はメモリ確保を避けること (prepare() で確保する)
///   - apply() は同じ入力に対して同じ出力を返すこと (決定的)
///   - apply() はスレッドセーフである必要はない (1 インスタンス = 1 スレッド)
///
/// ABI 安定性のため、仮想関数の追加は末尾のみ、既存の順序変更は禁止。
class ISubtitleEffect
{
public:
    virtual ~ISubtitleEffect() = default;

    /// 一意な識別子。プロジェクト JSON に保存される。一度公開したら変更してはならない。
    /// 命名: "<vendor>.<effect>" 例 "yave.typewriter" / "com.example.glitch"
    virtual QString id() const = 0;

    /// UI 表示名。翻訳キー ("effect.foo.name") を返してもよい。
    virtual QString displayName() const = 0;

    /// カテゴリ ("Transition" / "Motion" / "Color" / "Distort" / "AviUtl")
    virtual QString category() const = 0;

    /// パラメータ定義。UI はこれを見てフォームを自動生成する。
    virtual ParameterSchema parameterSchema() const = 0;

    /// クリップのテキスト / スタイル / 区間が確定したときに 1 回だけ呼ばれる。
    /// 重い前計算 (乱数テーブル、per-glyph の初期値) はここで行う。
    virtual void prepare(const SubtitleGlyphRun& run,
                         const ParameterValues& params,
                         const QSize& canvasSize)
    { Q_UNUSED(run); Q_UNUSED(params); Q_UNUSED(canvasSize); }

    /// 毎フレーム呼ばれる。
    virtual void apply(SubtitleEffectFrame& frame,
                       const SubtitleTimeInfo& time,
                       const ParameterValues& params) = 0;

    /// 独自 GUI を持つか。true なら PluginWindow に埋め込まれる。
    virtual bool     hasCustomEditor() const { return false; }
    virtual QWidget* createEditor(QWidget* parent) { Q_UNUSED(parent); return nullptr; }

    /// このエフェクトがブロック単位で動作するか (グリフ単位アニメーションと併用不可)。
    /// AviUtl アダプタが true を返す。
    virtual bool isBlockLevel() const { return false; }
};

} // namespace yave::sdk
