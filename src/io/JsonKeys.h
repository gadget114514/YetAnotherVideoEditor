#pragma once

#include <QString>

namespace yave::io::keys {

// キー文字列をリテラルで散らすと、タイプミスが実行時まで発覚しない。
// 定数として一箇所に集める。

inline constexpr auto kSchemaVersion   = "schemaVersion";
inline constexpr auto kProject         = "project";
inline constexpr auto kTracks          = "tracks";
inline constexpr auto kClips           = "clips";
inline constexpr auto kAiTasks         = "aiTasks";
inline constexpr auto kAssets          = "assets";

// メディアライブラリのフォルダ構成 (9.2.1)
inline constexpr auto kLibrary         = "library";
inline constexpr auto kFolders         = "folders";
inline constexpr auto kAssignments     = "assignments";
inline constexpr auto kParentId        = "parentId";

// Track
inline constexpr auto kTrackId       = "id";
inline constexpr auto kTrackName     = "name";
inline constexpr auto kTrackType     = "type";
inline constexpr auto kTrackVisible  = "visible";
inline constexpr auto kTrackLocked   = "locked";
inline constexpr auto kEffectChain   = "effectChain";   // トラック / マスターの音声チェーン専用
inline constexpr auto kTransitions   = "transitions";

// Transition (9.3.2)
inline constexpr auto kTransitionId  = "transitionId";
inline constexpr auto kFromClipId    = "fromClipId";
inline constexpr auto kToClipId      = "toClipId";
inline constexpr auto kCenterFrame   = "centerFrame";

// Clip
inline constexpr auto kClipId        = "id";
inline constexpr auto kClipType      = "type";
inline constexpr auto kEnabled       = "enabled";
inline constexpr auto kRange         = "range";
inline constexpr auto kRangeStart    = "start";
inline constexpr auto kRangeDuration = "duration";
inline constexpr auto kAssetId       = "assetId";
inline constexpr auto kSourceOffset  = "sourceOffset";
inline constexpr auto kFilters       = "filters";       // ビデオフィルタースタック (9.4.1)
inline constexpr auto kFilterId      = "filterId";
inline constexpr auto kPresetId      = "presetId";      // TitleClip (9.4.6)

// SubtitleClip
inline constexpr auto kText          = "text";
inline constexpr auto kTextPlain     = "plain";
inline constexpr auto kTextSpans     = "spans";
inline constexpr auto kStylePresetId = "stylePresetId";
inline constexpr auto kStyleOverride = "styleOverride";
inline constexpr auto kEffectStack   = "effectStack";
inline constexpr auto kWordTimings   = "wordTimings";

// SubtitleEffectInstance
inline constexpr auto kInstanceId    = "instanceId";
inline constexpr auto kEffectId      = "effectId";
inline constexpr auto kPluginId      = "pluginId";
inline constexpr auto kParams        = "params";

// AiGenerationParams
inline constexpr auto kKind           = "kind";
inline constexpr auto kTargetTrackId  = "targetTrackId";
inline constexpr auto kModelId        = "modelId";
inline constexpr auto kProviderId     = "providerId";
inline constexpr auto kPrompt         = "prompt";
inline constexpr auto kNegativePrompt = "negativePrompt";
inline constexpr auto kSeed           = "seed";
inline constexpr auto kVideoMode      = "videoMode";
inline constexpr auto kI2vRefMode     = "i2vRefMode";
inline constexpr auto kStartReference = "startReference";
inline constexpr auto kEndReference   = "endReference";
inline constexpr auto kVideoReference = "videoReference";
inline constexpr auto kExtraParams    = "extraParams";

/// クリップの既知キー一覧。未知フィールド保持 (9.11.2) の差分計算に使う。
QStringList knownClipKeys();
QStringList knownSubtitleClipKeys();

} // namespace yave::io::keys
