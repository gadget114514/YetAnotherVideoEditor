# windeployqt / macdeployqt wrapper. Usage: yave_deploy(target)
#
# Windows かつ FFmpeg 有効のときは、同梱 FFmpeg (third_party/ffmpeg) の
# 共有 DLL を実行ファイルの隣へコピーする。

function(yave_deploy target)
    if(WIN32)
        find_program(WINDEPLOYQT_EXE windeployqt HINTS "${QT_HOST_PATH}/bin" "$ENV{QTDIR}/bin")
        if(WINDEPLOYQT_EXE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                        --qmldir "${CMAKE_SOURCE_DIR}/src/app/qml"
                        "$<TARGET_FILE:${target}>"
                COMMENT "Running windeployqt for ${target}")
        else()
            message(STATUS "windeployqt not found; skipping deploy step for ${target}")
        endif()

        # 同梱 FFmpeg の共有 DLL を配布する (shared ビルドのため実行時に必要)
        if(YAVE_ENABLE_FFMPEG AND FFMPEG_ROOT)
            file(GLOB _ffmpeg_dlls "${FFMPEG_ROOT}/bin/*.dll")
            foreach(_dll ${_ffmpeg_dlls})
                get_filename_component(_dll_name "${_dll}" NAME)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                            "${_dll}" "$<TARGET_FILE_DIR:${target}>/${_dll_name}")
            endforeach()
            if(_ffmpeg_dlls)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E echo
                            "Deployed FFmpeg shared libraries for ${target}")
            endif()
            unset(_ffmpeg_dlls)
        endif()
    elseif(APPLE)
        find_program(MACDEPLOYQT_EXE macdeployqt HINTS "${QT_HOST_PATH}/bin")
        if(MACDEPLOYQT_EXE)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${MACDEPLOYQT_EXE}" "$<TARGET_FILE:${target}>"
                COMMENT "Running macdeployqt for ${target}")
        endif()
    endif()
endfunction()
