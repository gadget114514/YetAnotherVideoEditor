#pragma once

#include "AiGenerationTask.h"

#include <QJsonObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;

namespace yave {

class Project;

} // namespace yave

namespace yave::ai {

/// プロバイダが対応している機能の宣言。
struct ProviderCapability
{
    bool video        = false;
    bool audio        = false;
    bool subtitle     = false;
    bool image        = false;
    bool mask         = false;
    bool offline      = false;   ///< ネットワーク不要
    int  maxDurationFrames = -1; ///< -1 = 無制限

    bool supports(GenerationKind kind) const
    {
        switch (kind) {
        case GenerationKind::Video:          return video;
        case GenerationKind::Image:          return image;
        case GenerationKind::Audio:          return audio;
        case GenerationKind::Subtitle:       return subtitle;
        case GenerationKind::Mask:           return mask;
        case GenerationKind::EffectMetadata: return true;
        }
        return false;
    }
};

/// 生成の実行主体 (ローカル ONNX / リモート API / sidecar プロセス) の抽象。
///
/// 契約:
///   - run() はワーカスレッドから呼ばれる。UI に触れてはならない
///   - 進捗は progressFn 経由で報告する (0.0..1.0)。false が返ったら中断する
///   - 成果物は workDir へ書き、そのパスを GenerationOutput に入れて返す
struct GenerationInput
{
    AiGenerationParams params;
    QString            workDir;             ///< .yave_cache/gen/<task-uuid>/
    /// I2V 用に解決済みの参照画像。無ければ空
    QString            startRefPath;
    QString            endRefPath;
    /// V2V 用に書き出したソース映像。無ければ空
    QString            sourceVideoPath;
};

struct GenerationOutput
{
    bool                       ok = false;
    QString                    errorMessage;
    std::vector<GeneratedAsset> assets;
};

using ProgressFn  = std::function<bool(double)>;
using CancelCheckFn = std::function<bool()>;

class IGenerationProvider
{
public:
    virtual ~IGenerationProvider() = default;

    /// ProviderRegistry に登録するときの一意 id ("onnx-local" / "comfyui-local" 等)
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;

    virtual ProviderCapability capabilities() const = 0;

    /// このタスクを実行できるか (モデル存在確認など)。失敗理由を errorOut へ。
    virtual bool isAvailable(QString* errorOut = nullptr) const
    { Q_UNUSED(errorOut); return true; }

    virtual GenerationOutput run(const GenerationInput& input,
                                 const ProgressFn& progress,
                                 const CancelCheckFn& cancelled) = 0;

    /// タスク投入前のパラメータ補正 (既定 seed 決定など)
    virtual void prepareParams(AiGenerationParams&) {}
};

} // namespace yave::ai
