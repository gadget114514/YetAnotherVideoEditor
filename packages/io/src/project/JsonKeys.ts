export const kSchemaVersion = 'schemaVersion';
export const kProject = 'project';
export const kTracks = 'tracks';
export const kClips = 'clips';
export const kAiTasks = 'aiTasks';
export const kAssets = 'assets';

// Library
export const kLibrary = 'library';
export const kFolders = 'folders';
export const kAssignments = 'assignments';
export const kParentId = 'parentId';

// Track
export const kTrackId = 'id';
export const kTrackName = 'name';
export const kTrackType = 'type';
export const kTrackVisible = 'visible';
export const kTrackLocked = 'locked';
export const kEffectChain = 'effectChain';
export const kTransitions = 'transitions';

// Transition
export const kTransitionId = 'transitionId';
export const kFromClipId = 'fromClipId';
export const kToClipId = 'toClipId';
export const kCenterFrame = 'centerFrame';

// Clip
export const kClipId = 'id';
export const kClipType = 'type';
export const kEnabled = 'enabled';
export const kRange = 'range';
export const kRangeStart = 'start';
export const kRangeDuration = 'duration';
export const kAssetId = 'assetId';
export const kSourceOffset = 'sourceOffset';
export const kFilters = 'filters';
export const kFilterId = 'filterId';
export const kPresetId = 'presetId';

// SubtitleClip
export const kText = 'text';
export const kTextPlain = 'plain';
export const kTextSpans = 'spans';
export const kStylePresetId = 'stylePresetId';
export const kStyleOverride = 'styleOverride';
export const kEffectStack = 'effectStack';
export const kWordTimings = 'wordTimings';

// SubtitleEffectInstance
export const kInstanceId = 'instanceId';
export const kPluginId = 'pluginId';
export const kParams = 'params';

export const baseClipKeys: string[] = [
  kClipId,
  kClipType,
  kRange,
  'name',
  kEnabled,
  'locked',
  'opacity',
  'blendMode',
  'fadeIn',
  'fadeOut',
  'generatedByTaskId',
  kFilters,
];

export const videoClipKeys: string[] = [
  ...baseClipKeys,
  kAssetId,
  kSourceOffset,
  'speed',
  'reversed',
  'maxDuration',
];

export const subtitleClipKeys: string[] = [
  ...baseClipKeys,
  kStylePresetId,
  kText,
  kStyleOverride,
  kEffectStack,
  kWordTimings,
  kPresetId,
];
