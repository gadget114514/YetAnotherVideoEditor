#pragma once

#include <yave/sdk/ISubtitleEffect.h>

#include <QColor>
#include <QMatrix4x4>
#include <vector>

namespace yave::subtitle {

/// 組み込みエフェクトの共通基底。
/// id / displayName / parameterSchema は派生クラスが定義する。
class BuiltinEffectBase : public yave::sdk::ISubtitleEffect
{
public:
    QString category() const override { return QStringLiteral("Transition"); }
    bool hasCustomEditor() const override { return false; }
};

// ---------------------------------------------------------------------------
// yave.fade : フェードイン / アウト
// ---------------------------------------------------------------------------
class FadeEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.fade"); }
    QString displayName() const override { return QStringLiteral("effect.fade.name"); }
    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

// ---------------------------------------------------------------------------
// yave.typewriter : タイプライター (12.5.1 参照)
// ---------------------------------------------------------------------------
class TypewriterEffect final : public BuiltinEffectBase
{
public:
    TypewriterEffect() = default;

    QString id()          const override { return QStringLiteral("yave.typewriter"); }
    QString displayName() const override { return QStringLiteral("effect.typewriter.name"); }
    yave::sdk::ParameterSchema parameterSchema() const override;

    void prepare(const yave::sdk::SubtitleGlyphRun& run,
                 const yave::sdk::ParameterValues& params,
                 const QSize& canvasSize) override;

    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;

private:
    static double wordStartFor(int charIndex,
                               const std::vector<yave::sdk::SubtitleEffectFrame::WordTiming>& wt);

    // prepare() で確保する。apply() ではメモリ確保しない。
    std::vector<int> order_;
    int              maxOrder_ = 0;
};

// ---------------------------------------------------------------------------
// yave.karaoke : カラオケ (文字ごとに色が変わる)
// ---------------------------------------------------------------------------
class KaraokeEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.karaoke"); }
    QString displayName() const override { return QStringLiteral("effect.karaoke.name"); }
    QString category() const override { return QStringLiteral("Color"); }

    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

// ---------------------------------------------------------------------------
// yave.slidein : スライドイン
// ---------------------------------------------------------------------------
class SlideInEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.slidein"); }
    QString displayName() const override { return QStringLiteral("effect.slidein.name"); }
    QString category() const override { return QStringLiteral("Motion"); }

    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

// ---------------------------------------------------------------------------
// yave.popperchar : 文字ごとポップ
// ---------------------------------------------------------------------------
class PopPerCharEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.popperchar"); }
    QString displayName() const override { return QStringLiteral("effect.popperchar.name"); }
    QString category() const override { return QStringLiteral("Motion"); }

    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

// ---------------------------------------------------------------------------
// yave.wave : ウェーブ
// ---------------------------------------------------------------------------
class WaveEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.wave"); }
    QString displayName() const override { return QStringLiteral("effect.wave.name"); }
    QString category() const override { return QStringLiteral("Motion"); }

    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

// ---------------------------------------------------------------------------
// yave.blurin : ブラーイン
// ---------------------------------------------------------------------------
class BlurInEffect final : public BuiltinEffectBase
{
public:
    QString id() const override { return QStringLiteral("yave.blurin"); }
    QString displayName() const override { return QStringLiteral("effect.blurin.name"); }
    QString category() const override { return QStringLiteral("Transition"); }

    yave::sdk::ParameterSchema parameterSchema() const override;
    void apply(yave::sdk::SubtitleEffectFrame& frame,
               const yave::sdk::SubtitleTimeInfo& time,
               const yave::sdk::ParameterValues& params) override;
};

} // namespace yave::subtitle
