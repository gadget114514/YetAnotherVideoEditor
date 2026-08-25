set(YAVE_RENDER_SOURCES
    RhiContext.h
    RhiContext.cpp
    RhiCompositor.h
    RhiCompositor.cpp
    TexturePool.h
    TexturePool.cpp
    LayerPass.h
    LayerPass.cpp
    FilterPass.h
    FilterPass.cpp
    TransitionPass.h
    TransitionPass.cpp
    ColorSpace.h
    ColorSpace.cpp
)

add_library(yave_render STATIC ${YAVE_RENDER_SOURCES})
yave_apply_common_settings(yave_render)
target_link_libraries(yave_render
    PUBLIC yave_core Qt6::Core Qt6::Gui)

# QRhi は QtGui のセミパブリック API。プライベートヘッダパスが必要。
# Qt6Gui_PRIVATE_INCLUDE_DIRS が空の環境向けに、バージョン付きパスも明示的に足す。
target_include_directories(yave_render PRIVATE
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    "${Qt6Gui_DIR}/../../../include/QtGui/${Qt6Core_VERSION}/QtGui"
)

# ---- シェーダ (.vert/.frag -> .qsb、qrc 埋め込み) ------------------------
if(TARGET Qt6::ShaderTools)
    qt6_add_shaders(yave_render "yave_render_shaders"
        PREFIX "/shaders"
        BASE "shaders"
        FILES
            shaders/fullscreen.vert
            shaders/layer_blend.frag
            shaders/yuv_to_rgb.frag
            shaders/subtitle_glyph.vert
            shaders/subtitle_glyph.frag
            shaders/fullscreen_uv.vert
            shaders/filter_color.frag
            shaders/filter_blur.frag
            shaders/transition.frag
    )
else()
    message(WARNING "Qt6::ShaderTools not found; render shaders will be missing")
endif()

# ---- Windows: D3D11 interop ---------------------------------------------
if(WIN32)
    target_sources(yave_render PRIVATE ../platform/win/D3D11Interop.cpp)
endif()
