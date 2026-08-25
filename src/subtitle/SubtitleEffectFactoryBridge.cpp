#include "SubtitleEffectFactoryBridge.h"

namespace yave::subtitle {

namespace {
EffectFactoryFn g_factory;
}

void setEffectFactory(EffectFactoryFn fn)
{
    g_factory = std::move(fn);
}

EffectFactoryFn effectFactory()
{
    return g_factory;
}

} // namespace yave::subtitle
