add_library(yave_i18n STATIC
    LanguageManager.h
    LanguageManager.cpp
)
yave_apply_common_settings(yave_i18n)
target_link_libraries(yave_i18n PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml)
