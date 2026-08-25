set(YAVE_UTIL_SOURCES
    Log.h
    Log.cpp
    Result.h
    ScopeGuard.h
    Hash.h
    TimecodeFormat.h
    TimecodeFormat.cpp
)

add_library(yave_util STATIC ${YAVE_UTIL_SOURCES})
yave_apply_common_settings(yave_util)
target_link_libraries(yave_util PUBLIC Qt6::Core Qt6::Gui)
