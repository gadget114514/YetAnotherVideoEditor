set(YAVE_SUBTITLE_SOURCES
    SubtitleText.h
    SubtitleText.cpp
    SubtitleStyle.h
    SubtitleStylePreset.h
    SubtitleClip.h
    SubtitleClip.cpp
    TitleClip.h
    TitleClip.cpp
    SubtitleEffectInstance.h
    SubtitleEffectInstance.cpp
    SubtitleEffectFactoryBridge.h
    SubtitleEffectFactoryBridge.cpp
    SubtitleLayout.h
    SubtitleLayout.cpp

    commands/AddSubtitleEffectCommand.h
    commands/AddSubtitleEffectCommand.cpp

    effects/BuiltinEffects.h
    effects/BuiltinEffects.cpp

    io/SrtParser.h
    io/SrtParser.cpp
    io/SrtConverter.cpp
    io/SrtWriter.h
    io/SrtWriter.cpp
)

add_library(yave_subtitle STATIC ${YAVE_SUBTITLE_SOURCES})
yave_apply_common_settings(yave_subtitle)
target_link_libraries(yave_subtitle
    PUBLIC yave_core Qt6::Core Qt6::Gui)
target_include_directories(yave_subtitle PUBLIC ${CMAKE_SOURCE_DIR}/include)
