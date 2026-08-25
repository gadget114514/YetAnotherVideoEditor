#pragma once

#include "AiGenerationParams.h"
#include "AiGenerationTask.h"
#include "IGenerationProvider.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

#include <memory>
#include <vector>

namespace yave::ai {

/// 利用可能なプロバイダの管理と選択。
///
/// 登録される組み込みプロバイダ:
///   - RemoteHttpProvider  : OpenAI 互換 / ComfyUI / Replicate 等 (既定有効)
///   - SidecarProvider     : 子プロセス (ffmpeg パイプ等) 経由
///   - OnnxLocalProvider   : ONNX Runtime DirectML/CUDA/CoreML (YAVE_ENABLE_ONNX_LOCAL)
class ProviderRegistry
{
public:
    static ProviderRegistry& instance();

    void registerProvider(std::shared_ptr<IGenerationProvider> provider);
    std::vector<IGenerationProvider*> providers() const;

    /// id で検索。無ければ nullptr。
    IGenerationProvider* find(const QString& providerId) const;

    /// params を実行できる最初のプロバイダを選ぶ。
    /// params.providerId が空でなければそれを優先する。
    IGenerationProvider* selectFor(const AiGenerationParams& params) const;

private:
    ProviderRegistry();
    std::vector<std::shared_ptr<IGenerationProvider>> providers_;
};

} // namespace yave::ai
