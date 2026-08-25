# Compiler flags / warning levels shared by all YAVE targets.

function(yave_apply_common_settings target)
    target_compile_features(${target} PUBLIC cxx_std_17)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /Zc:__cplusplus /EHsc
            /wd4127   # conditional expression is constant (Qt headers)
            /wd4251   # dll-interface (Qt containers in interfaces)
        )
        target_compile_definitions(${target} PRIVATE
            NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
        if(WIN32)
            target_compile_definitions(${target} PRIVATE NOMINMAX)
        endif()
    endif()

    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "STATIC_LIBRARY" OR _type STREQUAL "OBJECT_LIBRARY")
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
    endif()
endfunction()
