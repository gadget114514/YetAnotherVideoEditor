#include "ProviderRegistry.h"

#include "providers/Providers.h"

#if defined(YAVE_ENABLE_ONNX_LOCAL)
#  include "providers/OnnxLocalProvider.h"
#endif

namespace yave::ai {

ProviderRegistry& ProviderRegistry::instance()
{
    static ProviderRegistry s;
    return s;
}

ProviderRegistry::ProviderRegistry()
{
    // 組み込みプロバイダの登録
    providers_.push_back(std::make_shared<RemoteHttpProvider>());
    providers_.push_back(std::make_shared<SidecarProvider>());
#if defined(YAVE_ENABLE_ONNX_LOCAL)
    providers_.push_back(std::make_shared<OnnxLocalProvider>());
#endif
}

void ProviderRegistry::registerProvider(std::shared_ptr<IGenerationProvider> provider)
{
    if (provider)
        providers_.push_back(std::move(provider));
}

std::vector<IGenerationProvider*> ProviderRegistry::providers() const
{
    std::vector<IGenerationProvider*> out;
    out.reserve(providers_.size());
    for (const auto& p : providers_)
        out.push_back(p.get());
    return out;
}

IGenerationProvider* ProviderRegistry::find(const QString& providerId) const
{
    for (const auto& p : providers_)
        if (p->id() == providerId)
            return p.get();
    return nullptr;
}

IGenerationProvider* ProviderRegistry::selectFor(const AiGenerationParams& params) const
{
    if (!params.providerId.isEmpty()) {
        if (auto* found = find(params.providerId))
            return found->capabilities().supports(params.kind) ? found : nullptr;
        return nullptr;
    }

    // 指定がなければ、対応している最初の利用可能プロバイダ
    for (const auto& p : providers_) {
        QString err;
        if (p->capabilities().supports(params.kind) && p->isAvailable(&err))
            return p.get();
    }
    return nullptr;
}

} // namespace yave::ai
