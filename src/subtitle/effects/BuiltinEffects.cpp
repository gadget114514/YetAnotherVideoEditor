#include "BuiltinEffects.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace yave::subtitle {

using namespace yave::sdk;

namespace {

ParamDef makeParam(ParamType type, const QString& key, const QString& displayKey,
                   QVariant def, QVariant min = {}, QVariant max = {},
                   QVariant step = {}, const QString& unit = {})
{
    ParamDef p;
    p.type           = type;
    p.key            = key;
    p.displayNameKey = displayKey;
    p.defaultValue   = std::move(def);
    p.minValue       = std::move(min);
    p.maxValue       = std::move(max);
    p.step           = std::move(step);
    p.unitSuffix     = unit;
    return p;
}

/// easeInOut カーブ (smoothstep)
double easeInOut(double t)
{
    t = qBound(0.0, t, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

int maxWordIndexOf(const SubtitleGlyphRun& run)
{
    int m = 0;
    for (const auto& g : run.glyphs)
        m = std::max(m, g.wordIndex);
    return m;
}

} // anonymous namespace

// ===========================================================================
//  Fade
// ===========================================================================

ParameterSchema FadeEffect::parameterSchema() const
{
    ParameterSchema s;
    s.push_back(makeParam(ParamType::Double, QStringLiteral("inDuration"),
                          QStringLiteral("effect.fade.inDuration"), 0.3,
                          0.0, 10.0, 0.05, QStringLiteral("s")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("outDuration"),
                          QStringLiteral("effect.fade.outDuration"), 0.3,
                          0.0, 10.0, 0.05, QStringLiteral("s")));
    s.push_back(makeParam(ParamType::Enum, QStringLiteral("curve"),
                          QStringLiteral("effect.fade.curve"), 1));
    s.back().enumKeys = { QStringLiteral("effect.fade.curve.linear"),
                          QStringLiteral("effect.fade.curve.easeInOut") };
    return s;
}

void FadeEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                       const ParameterValues& params)
{
    const double fadeIn  = params.getDouble(QStringLiteral("inDuration"), 0.3);
    const double fadeOut = params.getDouble(QStringLiteral("outDuration"), 0.3);
    const int    curve   = params.getInt(QStringLiteral("curve"), 1);

    double o = 1.0;
    if (fadeIn > 0.0 && time.secondsFromIn < fadeIn) {
        const double t = time.secondsFromIn / fadeIn;
        o = std::min(o, curve == 1 ? easeInOut(t) : t);
    }
    if (fadeOut > 0.0 && time.secondsToOut < fadeOut) {
        const double t = time.secondsToOut / fadeOut;
        o = std::min(o, curve == 1 ? easeInOut(t) : t);
    }

    if (frame.blockOpacity)
        *frame.blockOpacity *= float(o);
}

// ===========================================================================
//  Typewriter (12.5.1)
// ===========================================================================

ParameterSchema TypewriterEffect::parameterSchema() const
{
    ParameterSchema s;
    s.push_back(makeParam(ParamType::Double, QStringLiteral("charsPerSecond"),
                          QStringLiteral("effect.typewriter.charsPerSecond"), 18.0,
                          1.0, 120.0, 1.0, QStringLiteral("cps")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("startDelay"),
                          QStringLiteral("effect.typewriter.startDelay"), 0.0,
                          0.0, 10.0, 0.05, QStringLiteral("s")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("perCharFade"),
                          QStringLiteral("effect.typewriter.perCharFade"), 0.05,
                          0.0, 1.0, 0.01, QStringLiteral("s")));

    ParamDef mode;
    mode.key             = QStringLiteral("unit");
    mode.displayNameKey  = QStringLiteral("effect.typewriter.unit");
    mode.type            = ParamType::Enum;
    mode.defaultValue    = 0;
    mode.enumKeys        = { QStringLiteral("effect.typewriter.unit.char"),
                             QStringLiteral("effect.typewriter.unit.word"),
                             QStringLiteral("effect.typewriter.unit.line") };
    s.push_back(mode);

    ParamDef useStt;
    useStt.key            = QStringLiteral("followWordTimings");
    useStt.displayNameKey = QStringLiteral("effect.typewriter.followWordTimings");
    useStt.type           = ParamType::Bool;
    useStt.defaultValue   = false;
    s.push_back(useStt);

    return s;
}

void TypewriterEffect::prepare(const SubtitleGlyphRun& run,
                               const ParameterValues& params,
                               const QSize& canvasSize)
{
    Q_UNUSED(canvasSize);

    // apply() で確保しないよう、ここで作り切る。
    // 各グリフが「何番目に出現するか」を単位 (文字/単語/行) に応じて決める。
    const int unit = params.getInt(QStringLiteral("unit"), 0);

    order_.assign(run.glyphs.size(), 0);
    int maxOrder = 0;

    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        const GlyphInfo& g = run.glyphs[i];
        int idx = 0;
        switch (unit) {
        case 0: idx = g.charIndex; break;      ///< 文字単位
        case 1: idx = g.wordIndex; break;      ///< 単語単位
        case 2: idx = g.lineIndex; break;      ///< 行単位
        default: idx = g.charIndex; break;
        }
        order_[i] = idx;
        maxOrder  = std::max(maxOrder, idx);
    }
    maxOrder_ = maxOrder;
}

double TypewriterEffect::wordStartFor(
    int charIndex, const std::vector<SubtitleEffectFrame::WordTiming>& wt)
{
    for (const auto& w : wt) {
        if (charIndex >= w.charStart && charIndex < w.charStart + w.charLength)
            return w.startSec;
    }
    return 0.0;
}

void TypewriterEffect::apply(SubtitleEffectFrame& frame,
                             const SubtitleTimeInfo& time,
                             const ParameterValues& params)
{
    if (!frame.run || !frame.glyphs || order_.empty())
        return;

    const double cps       = std::max(0.001, params.getDouble(
                                 QStringLiteral("charsPerSecond"), 18.0));
    const double delay     = params.getDouble(QStringLiteral("startDelay"), 0.0);
    const double perFade   = std::max(0.0, params.getDouble(
                                 QStringLiteral("perCharFade"), 0.05));
    const bool   followStt = params.getBool(
                                 QStringLiteral("followWordTimings"), false);

    const double t = time.secondsFromIn - delay;

    auto& glyphs = *frame.glyphs;
    const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

    for (size_t i = 0; i < n; ++i) {
        const GlyphInfo& g = frame.run->glyphs[i];

        // このグリフが出現し始める時刻
        double appearAt;
        if (followStt && frame.wordTimings && !frame.wordTimings->empty())
            appearAt = wordStartFor(g.charIndex, *frame.wordTimings);
        else
            appearAt = double(order_[i]) / cps;

        const double dt = t - appearAt;

        if (dt < 0.0) {
            glyphs[i].visible = false;
            glyphs[i].opacity = 0.0f;
            continue;
        }

        glyphs[i].visible = true;
        glyphs[i].opacity = (perFade <= 0.0)
                              ? 1.0f
                              : float(std::min(1.0, dt / perFade));
    }
}

// ===========================================================================
//  Karaoke
// ===========================================================================

ParameterSchema KaraokeEffect::parameterSchema() const
{
    ParameterSchema s;
    ParamDef color;
    color.key           = QStringLiteral("highlightColor");
    color.displayNameKey= QStringLiteral("effect.karaoke.highlightColor");
    color.type          = ParamType::Color;
    color.defaultValue  = QColor(255, 220, 60);
    s.push_back(color);

    ParamDef mode;
    mode.key            = QStringLiteral("mode");
    mode.displayNameKey = QStringLiteral("effect.karaoke.mode");
    mode.type           = ParamType::Enum;
    mode.defaultValue   = 0;
    mode.enumKeys       = { QStringLiteral("effect.karaoke.mode.char"),
                            QStringLiteral("effect.karaoke.mode.word") };
    s.push_back(mode);

    s.push_back(makeParam(ParamType::Double, QStringLiteral("preRoll"),
                          QStringLiteral("effect.karaoke.preRoll"), 0.0,
                          0.0, 5.0, 0.05, QStringLiteral("s")));
    return s;
}

void KaraokeEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                          const ParameterValues& params)
{
    if (!frame.run || !frame.glyphs)
        return;

    const QColor highlight = params.getColor(QStringLiteral("highlightColor"),
                                             QColor(255, 220, 60));
    const int mode         = params.getInt(QStringLiteral("mode"), 0);
    const double preRoll   = params.getDouble(QStringLiteral("preRoll"), 0.0);
    auto& glyphs = *frame.glyphs;
    const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

    for (size_t i = 0; i < n; ++i) {
        const GlyphInfo& g = frame.run->glyphs[i];

        // STT の単語タイミングがあればそれを使い、無ければ均等割りにフォールバック
        double startSec = -1.0, endSec = -1.0;
        if (frame.wordTimings && !frame.wordTimings->empty()) {
            for (const auto& w : *frame.wordTimings) {
                if (g.charIndex >= w.charStart
                    && g.charIndex < w.charStart + w.charLength) {
                    startSec = w.startSec;
                    endSec   = w.endSec;
                    break;
                }
            }
        }
        if (startSec < 0.0) {
            const int unitIdx = (mode == 0) ? g.charIndex : g.wordIndex;
            const int units   = (mode == 0)
                                    ? int(frame.run->glyphs.size())
                                    : (maxWordIndexOf(*frame.run) + 1);
            const double perUnit = time.clipDurationSec / double(std::max(1, units));
            startSec = double(unitIdx) * perUnit - preRoll;
            endSec   = startSec + perUnit;
        }

        const double t = time.secondsFromIn;
        if (t >= startSec) {
            glyphs[i].color = highlight;
            glyphs[i].opacity =
                float(qBound(0.0, 1.0 - (t - endSec) / std::max(0.001, endSec - startSec) * 0.35, 1.0));
        }
    }
}

// ===========================================================================
//  SlideIn
// ===========================================================================

ParameterSchema SlideInEffect::parameterSchema() const
{
    ParameterSchema s;
    ParamDef dir;
    dir.key            = QStringLiteral("direction");
    dir.displayNameKey = QStringLiteral("effect.slidein.direction");
    dir.type           = ParamType::Enum;
    dir.defaultValue   = 0;
    dir.enumKeys       = { QStringLiteral("effect.slidein.dir.left"),
                           QStringLiteral("effect.slidein.dir.right"),
                           QStringLiteral("effect.slidein.dir.up"),
                           QStringLiteral("effect.slidein.dir.down") };
    s.push_back(dir);
    s.push_back(makeParam(ParamType::Double, QStringLiteral("distance"),
                          QStringLiteral("effect.slidein.distance"), 80.0,
                          0.0, 2000.0, 5.0, QStringLiteral("px")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("duration"),
                          QStringLiteral("effect.slidein.duration"), 0.4,
                          0.05, 5.0, 0.05, QStringLiteral("s")));
    return s;
}

void SlideInEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                          const ParameterValues& params)
{
    if (!frame.blockTransform)
        return;

    const int direction = params.getInt(QStringLiteral("direction"), 0);
    const double dist   = params.getDouble(QStringLiteral("distance"), 80.0);
    const double dur    = std::max(0.01, params.getDouble(QStringLiteral("duration"), 0.4));

    if (time.secondsFromIn >= dur)
        return;

    const double t = easeInOut(time.secondsFromIn / dur);   // 0..1
    const double remaining = (1.0 - t) * dist;

    QMatrix4x4 m;
    switch (direction) {
    case 0: m.translate(float(-remaining), 0.0f); break;
    case 1: m.translate(float(remaining), 0.0f);  break;
    case 2: m.translate(0.0f, float(-remaining)); break;
    case 3: m.translate(0.0f, float(remaining));  break;
    }

    *frame.blockTransform = m * (*frame.blockTransform);
}

// ===========================================================================
//  PopPerChar
// ===========================================================================

ParameterSchema PopPerCharEffect::parameterSchema() const
{
    ParameterSchema s;
    s.push_back(makeParam(ParamType::Double, QStringLiteral("stagger"),
                          QStringLiteral("effect.popperchar.stagger"), 0.04,
                          0.0, 1.0, 0.01, QStringLiteral("s")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("overshoot"),
                          QStringLiteral("effect.popperchar.overshoot"), 1.25,
                          1.0, 3.0, 0.05));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("duration"),
                          QStringLiteral("effect.popperchar.duration"), 0.25,
                          0.05, 2.0, 0.05, QStringLiteral("s")));
    return s;
}

void PopPerCharEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                             const ParameterValues& params)
{
    if (!frame.run || !frame.glyphs)
        return;

    const double stagger   = params.getDouble(QStringLiteral("stagger"), 0.04);
    const double overshoot = params.getDouble(QStringLiteral("overshoot"), 1.25);
    const double dur       = std::max(0.01, params.getDouble(QStringLiteral("duration"), 0.25));

    auto& glyphs = *frame.glyphs;
    const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

    for (size_t i = 0; i < n; ++i) {
        const GlyphInfo& g = frame.run->glyphs[i];
        if (g.isWhitespace)
            continue;

        const double appearAt = double(g.charIndex) * stagger;
        const double t = (time.secondsFromIn - appearAt) / dur;

        if (t < 0.0) {
            glyphs[i].visible = false;
            continue;
        }
        if (t >= 1.0)
            continue;

        // オーバーシュート付きスケールイン: sin カーブで 1 -> overshoot -> 1
        const double scale = 1.0 + (overshoot - 1.0) * std::sin(t * M_PI);
        QMatrix4x4 m;
        m.scale(float(scale), float(scale), 1.0f);
        glyphs[i].transform = m * glyphs[i].transform;
        glyphs[i].opacity  *= float(std::min(1.0, t * 3.0));
    }
}

// ===========================================================================
//  Wave
// ===========================================================================

ParameterSchema WaveEffect::parameterSchema() const
{
    ParameterSchema s;
    s.push_back(makeParam(ParamType::Double, QStringLiteral("amplitude"),
                          QStringLiteral("effect.wave.amplitude"), 8.0,
                          0.0, 200.0, 0.5, QStringLiteral("px")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("frequency"),
                          QStringLiteral("effect.wave.frequency"), 1.5,
                          0.1, 20.0, 0.1, QStringLiteral("Hz")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("speed"),
                          QStringLiteral("effect.wave.speed"), 1.0,
                          0.0, 10.0, 0.1));
    ParamDef axis;
    axis.key            = QStringLiteral("axis");
    axis.displayNameKey = QStringLiteral("effect.wave.axis");
    axis.type           = ParamType::Enum;
    axis.defaultValue   = 0;
    axis.enumKeys       = { QStringLiteral("effect.wave.axis.vertical"),
                            QStringLiteral("effect.wave.axis.horizontal") };
    s.push_back(axis);
    return s;
}

void WaveEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                       const ParameterValues& params)
{
    if (!frame.run || !frame.glyphs)
        return;

    const double amp   = params.getDouble(QStringLiteral("amplitude"), 8.0);
    const double freq  = params.getDouble(QStringLiteral("frequency"), 1.5);
    const double speed = params.getDouble(QStringLiteral("speed"), 1.0);
    const bool verticalAxis = params.getInt(QStringLiteral("axis"), 0) == 0;

    auto& glyphs = *frame.glyphs;
    const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

    const double phase = time.secondsFromIn * speed;
    for (size_t i = 0; i < n; ++i) {
        const GlyphInfo& g = frame.run->glyphs[i];
        if (g.isWhitespace)
            continue;
        const double offset = amp * std::sin(2.0 * M_PI * freq * phase
                                             + double(g.charIndex) * 0.45);
        QMatrix4x4 m;
        if (verticalAxis)
            m.translate(0.0f, float(offset));
        else
            m.translate(float(offset), 0.0f);
        glyphs[i].transform = m * glyphs[i].transform;
    }
}

// ===========================================================================
//  BlurIn
// ===========================================================================

ParameterSchema BlurInEffect::parameterSchema() const
{
    ParameterSchema s;
    s.push_back(makeParam(ParamType::Double, QStringLiteral("startBlur"),
                          QStringLiteral("effect.blurin.startBlur"), 16.0,
                          0.0, 100.0, 0.5, QStringLiteral("px")));
    s.push_back(makeParam(ParamType::Double, QStringLiteral("duration"),
                          QStringLiteral("effect.blurin.duration"), 0.5,
                          0.05, 5.0, 0.05, QStringLiteral("s")));
    return s;
}

void BlurInEffect::apply(SubtitleEffectFrame& frame, const SubtitleTimeInfo& time,
                         const ParameterValues& params)
{
    if (!frame.run || !frame.glyphs)
        return;

    const double startBlur = params.getDouble(QStringLiteral("startBlur"), 16.0);
    const double dur       = std::max(0.01, params.getDouble(QStringLiteral("duration"), 0.5));

    if (time.secondsFromIn >= dur)
        return;

    const double t = 1.0 - time.secondsFromIn / dur;    // 1 -> 0
    auto& glyphs = *frame.glyphs;
    const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());

    // 後ろの文字ほど遅れてシャープになる (波及)
    for (size_t i = 0; i < n; ++i) {
        const GlyphInfo& g = frame.run->glyphs[i];
        if (g.isWhitespace)
            continue;
        const double local = qBound(0.0, t - double(g.charIndex) * 0.02, 1.0);
        glyphs[i].blurRadius = float(startBlur * local);
        glyphs[i].opacity   *= float(1.0 - local * 0.6);
    }
}

} // namespace yave::subtitle
