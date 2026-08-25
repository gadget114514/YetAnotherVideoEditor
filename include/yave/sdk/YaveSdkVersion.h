#pragma once

#define YAVE_SDK_VERSION_MAJOR 1
#define YAVE_SDK_VERSION_MINOR 0
#define YAVE_SDK_VERSION_PATCH 0

inline constexpr int yaveSdkVersion() { return YAVE_SDK_VERSION_MAJOR * 10000 +
                                               YAVE_SDK_VERSION_MINOR * 100 +
                                               YAVE_SDK_VERSION_PATCH; }
