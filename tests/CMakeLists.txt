set(YAVE_TESTS
    tst_rational
    tst_timeline
    tst_srtparser
    tst_subtitlecommands
    tst_projectserializer
    tst_delaycompensator
    tst_i18n
    tst_transitionfilter
    tst_audiograph
    tst_propertycommands
)

foreach(test_name ${YAVE_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp)
    yave_apply_common_settings(${test_name})

    # テストは GPU もコーデックも無い環境で走るため、yave_media / yave_render は
    # リンクしない (2.2 モジュール依存関係の設計意図)。
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Test
        yave_core yave_subtitle yave_audio yave_io yave_i18n)

    if(test_name STREQUAL "tst_i18n")
        qt_add_translations(tst_i18n
            TS_FILES
                ${CMAKE_SOURCE_DIR}/i18n/yave_ja.ts
                ${CMAKE_SOURCE_DIR}/i18n/yave_en.ts
            RESOURCE_PREFIX "/i18n"
            LUPDATE_OPTIONS -no-obsolete -locations none
        )
    endif()

    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(WIN32)
        add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Test>"
            "$<TARGET_FILE_DIR:${test_name}>"
            VERBATIM)
    endif()
    add_test(NAME ${test_name}
             COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/data)
endforeach()

# ---- tst_mediaprobe -------------------------------------------------------
# MediaProbe は FFmpeg (yave_media) を必要とするため、FFmpeg 有効時のみビルドする。
# デコードはソフトウェア (libav) のみで GPU を必要としないため、ヘッドレスでも動作する。
set(YAVE_ALL_TESTS ${YAVE_TESTS})
if(YAVE_ENABLE_FFMPEG)
    qt_add_executable(tst_mediaprobe tst_mediaprobe.cpp)
    yave_apply_common_settings(tst_mediaprobe)
    target_link_libraries(tst_mediaprobe PRIVATE
        Qt6::Core Qt6::Gui Qt6::Test
        yave_core yave_media)
    set_target_properties(tst_mediaprobe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(WIN32)
        add_custom_command(TARGET tst_mediaprobe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Test>" "$<TARGET_FILE_DIR:tst_mediaprobe>"
            VERBATIM)
    endif()
    add_test(NAME tst_mediaprobe COMMAND tst_mediaprobe
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/data)
    list(APPEND YAVE_ALL_TESTS tst_mediaprobe)
endif()

set_tests_properties(${YAVE_ALL_TESTS} PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}")

