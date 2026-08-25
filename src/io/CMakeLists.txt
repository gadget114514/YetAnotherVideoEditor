set(YAVE_IO_SOURCES
    JsonKeys.h
    EnumMapping.h
    PathResolver.h
    PathResolver.cpp
    SchemaMigration.h
    SchemaMigration.cpp
    ProjectSerializer.h
    ProjectSerializer.cpp
)

add_library(yave_io STATIC ${YAVE_IO_SOURCES})
yave_apply_common_settings(yave_io)
target_link_libraries(yave_io
    PUBLIC yave_core yave_subtitle yave_ai Qt6::Core Qt6::Gui)
