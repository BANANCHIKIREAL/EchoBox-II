cmake_minimum_required(VERSION 3.16)
project(EchoBoxII VERSION 1.5.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)

set(APP_ICON_SOURCES "")
if (MINGW)
    # CMake's built-in .rc handling feeds every -I of the target (including
    # the AUTOMOC/Qt include dirs) into windres' internal preprocessor call,
    # which chokes on paths containing spaces (this project lives under
    # "VS Code pojects"). The .rc has no preprocessor directives at all, so
    # just invoke windres directly with none of that.
    find_program(WINDRES_EXECUTABLE NAMES windres REQUIRED)
    set(APP_ICON_RC_OBJ "${CMAKE_CURRENT_BINARY_DIR}/app_icon_rc.o")
    add_custom_command(
        OUTPUT  "${APP_ICON_RC_OBJ}"
        COMMAND "${WINDRES_EXECUTABLE}" -O coff -i app_icon.rc -o "${APP_ICON_RC_OBJ}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/app_icon.rc" "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
        COMMENT "Compiling app icon resource"
    )
    set_source_files_properties("${APP_ICON_RC_OBJ}" PROPERTIES
        EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set(APP_ICON_SOURCES "${APP_ICON_RC_OBJ}")
endif()

qt_add_executable(EchoBoxII
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/visualizer.cpp
    src/visualizer.h
    src/backgroundwidget.cpp
    src/backgroundwidget.h
    src/logo.h
    src/icons.h
    src/discordrpc.h
    src/discordrpc.cpp
    src/settingsdialog.h
    src/settingsdialog.cpp
    src/waveformslider.h
    src/waveformslider.cpp
    src/libraryscanner.h
    src/libraryscanner.cpp
    ${APP_ICON_SOURCES}
)

target_link_libraries(EchoBoxII PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

set_target_properties(EchoBoxII PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE   TRUE
)
