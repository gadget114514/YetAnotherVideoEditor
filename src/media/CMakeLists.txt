set(YAVE_MEDIA_SOURCES
    MediaInfo.h
    MediaProbe.h
    MediaProbe.cpp
    FFmpegRaii.h
    VideoDecoder.h
    VideoDecoder.cpp
    HwDeviceContext.h
    HwDeviceContext.cpp
    FrameCache.h
    FrameCache.cpp
    FrameQueue.h
    DecodeWorkerPool.h
    DecodeWorkerPool.cpp
    ExportJob.h
    ExportJob.cpp
)

add_library(yave_media STATIC ${YAVE_MEDIA_SOURCES})
yave_apply_common_settings(yave_media)
target_link_libraries(yave_media PUBLIC yave_core Qt6::Core Qt6::Gui)

if(YAVE_ENABLE_FFMPEG)
    target_compile_definitions(yave_media PUBLIC YAVE_HAVE_FFMPEG=1)
    target_link_libraries(yave_media PUBLIC
        FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil
        FFmpeg::swscale FFmpeg::swresample FFmpeg::avfilter)

    if(MSVC)
        target_compile_options(yave_media PRIVATE /wd4819)   # BOM 無しソース警告の抑止
    endif()
endif()

target_link_libraries(yave_media PRIVATE Qt6::Concurrent)
