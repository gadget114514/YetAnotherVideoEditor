#pragma once

#include "ISubtitleEffect.h"
#include "YaveSdkVersion.h"
#include "YaveExport.h"

namespace yave::sdk {

inline constexpr int kSubtitleEffectApiVersion = 1;

/// 外部字幕エフェクトプラグインがエクスポートするファクトリ構造体。
/// ABI 安定性のため、既存メンバの順序変更・削除は禁止。追加は末尾のみ。
struct SubtitleEffectFactoryV1
{
    int  apiVersion;                                  // kSubtitleEffectApiVersion
    const char* pluginId;                             // "com.example.glitch"
    const char* pluginDisplayName;
    const char* pluginVersion;                        // "1.0.0"
    const char* translationQmPrefix;                  // 同梱 .qm の接頭辞。無ければ nullptr

    int  (*effectCount)();
    ISubtitleEffect* (*createEffect)(int index);      ///< 呼び出し側が destroyEffect で解放
    void (*destroyEffect)(ISubtitleEffect*);
    void (*shutdown)();                               ///< アンロード直前に呼ばれる
};

} // namespace yave::sdk

extern "C" YAVE_PLUGIN_EXPORT
const yave::sdk::SubtitleEffectFactoryV1* yaveCreateSubtitleEffectFactory();
