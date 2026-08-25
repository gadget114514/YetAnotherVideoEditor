// 字幕エフェクトプラグインのサンプル (GlitchEffect)。
//
// yaveCreateSubtitleEffectFactory() だけをエクスポートし、
// SubtitleEffectFactoryV1 構造体経由でエフェクト実装を提供する。
// プラグイン作者はこのファイルを雛形にしてよい。

#include <yave/sdk/SubtitleEffectApi.h>
#include <yave/sdk/ParameterSchema.h>

#include <QMatrix4x4>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

using namespace yave::sdk;

/// グリフ単位のランダム変位でグリッチ風の揺れを作るサンプル。
/// prepare() で乱数テーブルを作り、apply() では決定的な参照のみ行う
/// (契約: 同じ入力 -> 同じ出力)。
class GlitchEffect final : public ISubtitleEffect
{
public:
    QString id() const override { return QStringLiteral("yave.example.glitch"); }
    QString displayName() const override { return QStringLiteral("Glitch (example)"); }
    QString category() const override { return QStringLiteral("Distort"); }

    ParameterSchema parameterSchema() const override
    {
        ParameterSchema s;

        ParamDef intensity;
        intensity.key            = "intensity";
        intensity.displayNameKey = "Intensity";
        intensity.type           = ParamType::Double;
        intensity.defaultValue   = 0.4;
        intensity.minValue       = 0.0;
        intensity.maxValue       = 1.0;
        intensity.step           = 0.05;
        s.push_back(intensity);

        ParamDef speed;
        speed.key            = "speed";
        speed.displayNameKey = "Speed";
        speed.type           = ParamType::Double;
        speed.defaultValue   = 8.0;
        speed.minValue       = 1.0;
        speed.maxValue       = 60.0;
        s.push_back(speed);

        ParamDef seed;
        seed.key            = "seed";
        seed.displayNameKey = "Seed";
        seed.type           = ParamType::Int;
        seed.defaultValue   = 12345;
        s.push_back(seed);

        return s;
    }

    void prepare(const SubtitleGlyphRun& run,
                 const ParameterValues& params,
                 const QSize& canvasSize) override
    {
        Q_UNUSED(canvasSize);

        // apply() 内で確保しないため、ここですべて作り切る。
        const int seed = params.getInt("seed", 12345);
        jitterX_.assign(run.glyphs.size(), 0.0f);
        jitterY_.assign(run.glyphs.size(), 0.0f);
        splitFrames_.assign(run.glyphs.size(), 0);

        // 単純な LCG。プラットフォーム間で同一結果になるよう自前実装を使う。
        uint32_t state = uint32_t(seed) | 1u;
        auto next = [&state]() {
            state = state * 1664525u + 1013904223u;
            return float(state >> 16) / float(0xFFFF);
        };

        for (size_t i = 0; i < run.glyphs.size(); ++i) {
            jitterX_[i]     = next() * 2.0f - 1.0f;
            jitterY_[i]     = next() * 2.0f - 1.0f;
            splitFrames_[i] = int(next() * 4.0f);   ///< 4 フレーム周期の位相
        }
    }

    void apply(SubtitleEffectFrame& frame,
               const SubtitleTimeInfo& time,
               const ParameterValues& params) override
    {
        if (!frame.run || !frame.glyphs || jitterX_.empty())
            return;

        const double intensity = params.getDouble("intensity", 0.4);
        if (intensity <= 0.0)
            return;
        const double speed = std::max(0.01, params.getDouble("speed", 8.0));

        auto& glyphs = *frame.glyphs;
        const size_t n = std::min(glyphs.size(), frame.run->glyphs.size());
        const double phase = time.secondsFromIn * speed;

        for (size_t i = 0; i < n; ++i) {
            const GlyphInfo& g = frame.run->glyphs[i];
            if (g.isWhitespace)
                continue;

            // 位相が切り替わる瞬間だけ大きく跳ねる (ステップ関数)
            const double tick = std::floor(phase + double(splitFrames_[i]));
            const bool active = std::fmod(tick, 3.0) < 1.0;   ///< 3 ティック中 1 有効
            if (!active)
                continue;

            QMatrix4x4 m;
            m.translate(float(jitterX_[size_t(i)]) * float(intensity * 12.0),
                        float(jitterY_[size_t(i)]) * float(intensity * 6.0));
            glyphs[size_t(i)].transform = m * glyphs[size_t(i)].transform;

            // 色ズレ表現: R を強調
            QColor c = glyphs[size_t(i)].color;
            c.setRedF(std::min(1.0, double(c.redF()) + intensity * 0.3));
            glyphs[size_t(i)].color = c;
        }
    }

private:
    std::vector<float> jitterX_;
    std::vector<float> jitterY_;
    std::vector<int>   splitFrames_;
};

std::unique_ptr<ISubtitleEffect> makeEffect()
{
    return std::make_unique<GlitchEffect>();
}

} // anonymous namespace

extern "C" YAVE_PLUGIN_EXPORT
const yave::sdk::SubtitleEffectFactoryV1* yaveCreateSubtitleEffectFactory();

const yave::sdk::SubtitleEffectFactoryV1* yaveCreateSubtitleEffectFactory()
{
    static const yave::sdk::SubtitleEffectFactoryV1 factory = {
        /* apiVersion          */ yave::sdk::kSubtitleEffectApiVersion,
        /* pluginId            */ "yave.example.glitch",
        /* pluginDisplayName   */ "YAVE Example Glitch",
        /* pluginVersion       */ "1.0.0",
        /* translationQmPrefix */ nullptr,

        /* effectCount         */ [] { return 1; },
        /* createEffect        */
        [](int index) -> yave::sdk::ISubtitleEffect* {
            if (index != 0)
                return nullptr;
            return new ::GlitchEffect();
        },
        /* destroyEffect       */
        [](yave::sdk::ISubtitleEffect* e) { delete e; },
        /* shutdown            */
        [] {},
    };
    return &factory;
}
