add_executable(yave_launcher WIN32
    main.cpp
    launcher.rc
)

yave_apply_common_settings(yave_launcher)

set_target_properties(yave_launcher PROPERTIES
    OUTPUT_NAME "yave"
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(yave_launcher PRIVATE
    user32
    shell32
    shlwapi
)

# Size optimization for minimal binary footprint
if(MSVC)
    target_compile_options(yave_launcher PRIVATE /Os)
else()
    target_compile_options(yave_launcher PRIVATE -Os)
    target_link_options(yave_launcher PRIVATE -s -static)
endif()

# Also copy the launcher into bin/ directory so it can be invoked from either root or bin
add_custom_command(TARGET yave_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:yave_launcher>"
        "${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:yave_launcher>"
    COMMENT "Copying yave launcher to bin/"
)
