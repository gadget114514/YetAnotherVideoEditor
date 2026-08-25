set(YAVE_TESTS
    tst_rational
    tst_timeline
    tst_srtparser
    tst_projectserializer
    tst_delaycompensator
    tst_i18n
    tst_transitionfilter
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

set_tests_properties(${YAVE_TESTS} PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}")

