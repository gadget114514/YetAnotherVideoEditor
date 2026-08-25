set(YAVE_AI_SOURCES
    AiGenerationParams.h
    AiGenerationParams.cpp
    AiGenerationTask.h
    AiGenerationTask.cpp
    AiGenerationOrchestrator.h
    AiGenerationOrchestrator.cpp
    AiOrchestratorPersistence.cpp
    IGenerationProvider.h
    ProviderRegistry.h
    ProviderRegistry.cpp
    GenerationCache.h
    GenerationCache.cpp

    providers/Providers.h
    providers/Providers.cpp
)

add_library(yave_ai STATIC ${YAVE_AI_SOURCES})
yave_apply_common_settings(yave_ai)
target_link_libraries(yave_ai
    PUBLIC yave_core yave_subtitle Qt6::Core Qt6::Network Qt6::Concurrent)

if(YAVE_ENABLE_ONNX_LOCAL)
    target_compile_definitions(yave_ai PUBLIC YAVE_ENABLE_ONNX_LOCAL=1)
    # find_package(onnxruntime) が通る環境でのみリンクする
    find_package(onnxruntime QUIET)
    if(onnxruntime_FOUND)
        target_link_libraries(yave_ai PRIVATE onnxruntime::onnxruntime)
    else()
        message(STATUS "onnxruntime not found; OnnxLocalProvider will report unavailable")
        target_compile_definitions(yave_ai PRIVATE YAVE_ONNX_RUNTIME_MISSING=1)
    endif()
endif()
